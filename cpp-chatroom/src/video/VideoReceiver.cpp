#include "video/VideoReceiver.h"
#include "video/VideoFrame.h"
#include "ipmsg.h"
#include <QDebug>
#include <QDateTime>
#include <QBuffer>
#include <QImageReader>
#include <algorithm>

VideoReceiver::VideoReceiver(QObject* parent)
    : QObject(parent)
{
}

VideoReceiver::~VideoReceiver()
{
    stopListening();
}

bool VideoReceiver::startListening(uint16_t port)
{
    QMutexLocker locker(&m_mutex);

    if (m_listening) {
        return true;
    }

    if (!m_socket) {
        m_socket = new QUdpSocket(this);
    }

    if (!m_socket->bind(QHostAddress::Any, port)) {
        qDebug() << "[VideoReceiver] 绑定端口失败:" << m_socket->errorString();
        emit receiveError("绑定端口失败: " + m_socket->errorString());
        return false;
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &VideoReceiver::onReadyRead);

    // 超时检查定时器
    m_timeoutTimer = new QTimer(this);
    connect(m_timeoutTimer, &QTimer::timeout, this, &VideoReceiver::onTimeoutCheck);
    m_timeoutTimer->start(50); // 50ms检查一次

    m_listening = true;
    m_lastStatsTime = QDateTime::currentMSecsSinceEpoch();
    m_framesReceived = 0;
    m_framesLost = 0;

    qDebug() << "[VideoReceiver] 开始监听端口" << port;
    emit listeningStarted(port);

    return true;
}

void VideoReceiver::stopListening()
{
    QMutexLocker locker(&m_mutex);

    if (!m_listening) {
        return;
    }

    m_listening = false;

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        delete m_timeoutTimer;
        m_timeoutTimer = nullptr;
    }

    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }

    m_frameBuffers.clear();

    qDebug() << "[VideoReceiver] 停止监听";
    emit listeningStopped();
}

void VideoReceiver::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort;

        m_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        processChunk(sender, datagram);
    }
}

void VideoReceiver::processChunk(const QHostAddress& sender, const QByteArray& data)
{
    if (data.size() < static_cast<int>(sizeof(VideoChunkHeader))) {
        return;
    }

    VideoChunkHeader header;
    if (!parseChunkHeader(reinterpret_cast<const uint8_t*>(data.constData()),
                          static_cast<size_t>(data.size()), header)) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    // 获取或创建帧缓冲
    auto& buffer = m_frameBuffers[header.frameId];
    if (buffer.frameId == 0) {
        buffer.frameId = header.frameId;
        buffer.totalChunks = header.totalChunks;
        buffer.chunks.resize(header.totalChunks);
        buffer.received.resize(header.totalChunks, false);
        buffer.firstChunkTime = QDateTime::currentMSecsSinceEpoch();
        buffer.senderIp = sender.toString();
    }

    // 存储分片
    if (header.chunkIndex < buffer.totalChunks && !buffer.received[header.chunkIndex]) {
        const uint8_t* payload = reinterpret_cast<const uint8_t*>(data.constData()) + sizeof(VideoChunkHeader);
        size_t payloadSize = static_cast<size_t>(data.size()) - sizeof(VideoChunkHeader);
        
        buffer.chunks[header.chunkIndex].assign(payload, payload + payloadSize);
        buffer.received[header.chunkIndex] = true;

        // 检查是否完整
        if (buffer.isComplete()) {
            locker.unlock();
            completeFrame(header.frameId);
        }
    }
}

void VideoReceiver::completeFrame(uint32_t frameId)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_frameBuffers.find(frameId);
    if (it == m_frameBuffers.end()) {
        return;
    }

    auto& buffer = it->second;
    std::vector<uint8_t> jpegData = buffer.assemble();
    QString senderIp = buffer.senderIp;

    // 解码JPEG
    QByteArray ba(reinterpret_cast<const char*>(jpegData.data()), 
                  static_cast<int>(jpegData.size()));
    QBuffer qbuffer(&ba);
    qbuffer.open(QIODevice::ReadOnly);
    
    QImageReader reader(&qbuffer, "JPEG");
    QImage image = reader.read();

    // 清理此帧
    m_frameBuffers.erase(it);
    m_lastCompletedFrame = frameId;
    ++m_framesReceived;

    locker.unlock();

    if (!image.isNull()) {
        emit frameReceived(image, frameId, senderIp);
    }
}

void VideoReceiver::onTimeoutCheck()
{
    QMutexLocker locker(&m_mutex);

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 清理超时帧
    std::vector<uint32_t> toRemove;
    for (auto& [frameId, buffer] : m_frameBuffers) {
        if (now - buffer.firstChunkTime > VIDEO_FRAME_TIMEOUT_MS) {
            toRemove.push_back(frameId);
            ++m_framesLost;
        }
    }

    for (uint32_t id : toRemove) {
        m_frameBuffers.erase(id);
    }

    // 每秒更新统计
    if (now - m_lastStatsTime >= 1000) {
        int fps = m_framesReceived;
        m_framesReceived = 0;
        m_lastStatsTime = now;
        
        locker.unlock();
        emit statsUpdated(fps, m_framesLost);
        m_framesLost = 0;
    }
}

void VideoReceiver::cleanupOldFrames()
{
    // 清理早于最后完成帧的缓冲
    std::vector<uint32_t> toRemove;
    for (auto& [frameId, buffer] : m_frameBuffers) {
        if (frameId < m_lastCompletedFrame) {
            toRemove.push_back(frameId);
        }
    }
    for (uint32_t id : toRemove) {
        m_frameBuffers.erase(id);
    }
}

bool VideoReceiver::FrameBuffer::isComplete() const
{
    return std::all_of(received.begin(), received.end(), [](bool b) { return b; });
}

std::vector<uint8_t> VideoReceiver::FrameBuffer::assemble() const
{
    std::vector<uint8_t> result;
    for (const auto& chunk : chunks) {
        result.insert(result.end(), chunk.begin(), chunk.end());
    }
    return result;
}
