#include "video/VideoStreamer.h"
#include "video/VideoFrame.h"
#include "ipmsg.h"
#include <QDebug>
#include <chrono>

VideoStreamer::VideoStreamer(QObject* parent)
    : QObject(parent)
{
}

VideoStreamer::~VideoStreamer()
{
    stopStreaming();
}

bool VideoStreamer::startStreaming(const QString& targetIp, uint16_t targetPort)
{
    QMutexLocker locker(&m_mutex);

    if (m_streaming) {
        return true;
    }

    if (!m_socket) {
        m_socket = new QUdpSocket(this);
    }

    m_targetAddress = QHostAddress(targetIp);
    m_targetPort = targetPort;
    m_targetIp = targetIp;
    m_frameId = 0;
    m_streaming = true;

    qDebug() << "[VideoStreamer] 开始向" << targetIp << ":" << targetPort << "推流";
    emit streamingStarted(targetIp);

    return true;
}

void VideoStreamer::stopStreaming()
{
    QMutexLocker locker(&m_mutex);

    if (!m_streaming) {
        return;
    }

    m_streaming = false;

    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }

    qDebug() << "[VideoStreamer] 停止推流";
    emit streamingStopped();
}

bool VideoStreamer::sendFrame(const std::vector<uint8_t>& jpegData, uint16_t width, uint16_t height)
{
    if (!m_streaming || !m_socket) {
        return false;
    }

    if (jpegData.empty()) {
        return false;
    }

    uint32_t frameId = m_frameId++;

    // 将帧拆分为UDP分包
    auto chunks = splitFrameToChunks(frameId, jpegData, VIDEO_MAX_CHUNK_SIZE);

    if (chunks.empty()) {
        emit sendError("帧分包失败");
        return false;
    }

    // 发送所有分包
    int sentCount = 0;
    for (const auto& chunk : chunks) {
        qint64 sent = m_socket->writeDatagram(
            reinterpret_cast<const char*>(chunk.data()),
            static_cast<qint64>(chunk.size()),
            m_targetAddress,
            m_targetPort
        );

        if (sent < 0) {
            qDebug() << "[VideoStreamer] UDP发送失败:" << m_socket->errorString();
        } else {
            ++sentCount;
        }
    }

    if (sentCount == static_cast<int>(chunks.size())) {
        emit frameSent(frameId, sentCount);
        return true;
    } else {
        emit sendError(QString("帧 %1 部分发送失败 (%2/%3)")
                       .arg(frameId)
                       .arg(sentCount)
                       .arg(chunks.size()));
        return false;
    }
}
