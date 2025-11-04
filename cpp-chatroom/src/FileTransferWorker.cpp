#include "FileTransferWorker.h"
#include <QHostAddress>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

FileTransferWorker::FileTransferWorker(QObject* parent)
    : QObject(parent)
    , m_mode(None)
    , m_tcpServer(nullptr)
    , m_tcpSocket(nullptr)
    , m_file(nullptr)
    , m_fileSize(0)
    , m_bytesTransferred(0)
    , m_expectedSize(0)
{
}

FileTransferWorker::~FileTransferWorker()
{
    cleanup();
}

void FileTransferWorker::startReceiving(const QString& savePath, qint64 expectedSize)
{
    if (m_mode != None) {
        emit receiveError("传输正在进行中，无法开始新的接收任务");
        return;
    }
    
    m_mode = Receiving;
    m_filePath = savePath;
    m_expectedSize = expectedSize;
    m_bytesTransferred = 0;
    
    setupReceiving();
}

void FileTransferWorker::startSending(const QString& filePath, const QString& host, quint16 port)
{
    if (m_mode != None) {
        emit sendError("传输正在进行中，无法开始新的发送任务");
        return;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit sendError(QString("文件不存在: %1").arg(filePath));
        return;
    }
    
    m_mode = Sending;
    m_filePath = filePath;
    m_fileSize = fileInfo.size();
    m_bytesTransferred = 0;
    
    setupSending();
    
    // 连接到目标主机
    m_tcpSocket->connectToHost(QHostAddress(host), port);
}

void FileTransferWorker::setupReceiving()
{
    cleanup();
    
    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &FileTransferWorker::onNewConnection);
    
    // 绑定到0端口，系统自动分配
    if (!m_tcpServer->listen(QHostAddress::Any, 0)) {
        emit receiveError(QString("无法启动TCP服务器: %1").arg(m_tcpServer->errorString()));
        cleanup();
        return;
    }
    
    quint16 port = m_tcpServer->serverPort();
    qDebug() << QString("TCP服务器已启动，监听端口: %1").arg(port);
    
    // 确保保存目录存在
    QFileInfo fileInfo(m_filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    emit readyToReceive(port, m_filePath);
}

void FileTransferWorker::setupSending()
{
    cleanup();
    
    m_tcpSocket = new QTcpSocket(this);
    connect(m_tcpSocket, &QTcpSocket::connected, this, &FileTransferWorker::onClientConnected);
    connect(m_tcpSocket, &QTcpSocket::bytesWritten, this, &FileTransferWorker::onClientBytesWritten);
    connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &FileTransferWorker::onSocketError);
    
    m_file = new QFile(m_filePath, this);
    if (!m_file->open(QIODevice::ReadOnly)) {
        emit sendError(QString("无法打开文件: %1").arg(m_file->errorString()));
        cleanup();
        return;
    }
}

void FileTransferWorker::onNewConnection()
{
    if (!m_tcpServer || m_mode != Receiving) {
        return;
    }
    
    m_tcpSocket = m_tcpServer->nextPendingConnection();
    if (!m_tcpSocket) {
        return;
    }
    
    m_tcpServer->close();
    
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &FileTransferWorker::onClientReadyRead);
    connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &FileTransferWorker::onSocketError);
    
    // 打开文件准备写入
    m_file = new QFile(m_filePath, this);
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit receiveError(QString("无法创建文件: %1").arg(m_file->errorString()));
        cleanup();
        return;
    }
    
    qDebug() << QString("开始接收文件: %1").arg(m_filePath);
}

void FileTransferWorker::onClientConnected()
{
    if (m_mode != Sending || !m_file) {
        return;
    }
    
    qDebug() << QString("已连接到服务器，开始发送文件: %1").arg(m_filePath);
    
    // 开始发送数据
    const qint64 chunkSize = 64 * 1024; // 64KB chunks
    QByteArray buffer = m_file->read(chunkSize);
    if (!buffer.isEmpty()) {
        m_tcpSocket->write(buffer);
        m_bytesTransferred += buffer.size();
        emit sendProgress(m_bytesTransferred, m_fileSize);
    }
}

void FileTransferWorker::onClientReadyRead()
{
    if (m_mode != Receiving || !m_file || !m_tcpSocket) {
        return;
    }
    
    QByteArray data = m_tcpSocket->readAll();
    if (data.isEmpty()) {
        // 检查socket是否已断开
        if (m_tcpSocket->state() == QAbstractSocket::UnconnectedState) {
            // Socket已断开，检查是否接收完成
            if (m_bytesTransferred >= m_expectedSize) {
                m_file->close();
                emit receiveFinished(m_filePath);
                cleanup();
            } else {
                emit receiveError(QString("连接意外断开，已接收 %1/%2 字节")
                    .arg(m_bytesTransferred).arg(m_expectedSize));
                cleanup();
            }
        }
        return;
    }
    
    qint64 bytesWritten = m_file->write(data);
    if (bytesWritten != data.size()) {
        emit receiveError(QString("写入文件失败: %1").arg(m_file->errorString()));
        cleanup();
        return;
    }
    
    m_bytesTransferred += bytesWritten;
    emit receiveProgress(m_bytesTransferred, m_expectedSize);
    
    // 检查是否接收完成
    if (m_bytesTransferred >= m_expectedSize) {
        m_file->close();
        // 等待socket关闭
        m_tcpSocket->disconnectFromHost();
        if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
            m_tcpSocket->waitForDisconnected(3000);
        }
        emit receiveFinished(m_filePath);
        cleanup();
    }
}

void FileTransferWorker::onClientBytesWritten(qint64 bytes)
{
    if (m_mode != Sending || !m_file) {
        return;
    }
    
    // 继续发送下一块数据
    if (m_bytesTransferred < m_fileSize) {
        const qint64 chunkSize = 64 * 1024; // 64KB chunks
        QByteArray buffer = m_file->read(chunkSize);
        if (!buffer.isEmpty()) {
            m_tcpSocket->write(buffer);
            m_bytesTransferred += buffer.size();
            emit sendProgress(m_bytesTransferred, m_fileSize);
        } else {
            // 文件读取完成
            m_file->close();
            m_tcpSocket->disconnectFromHost();
            emit sendFinished();
            cleanup();
        }
    }
}

void FileTransferWorker::onSocketError(QAbstractSocket::SocketError error)
{
    QString errorMsg;
    if (m_mode == Receiving) {
        errorMsg = QString("接收文件时发生错误: %1").arg(m_tcpSocket ? m_tcpSocket->errorString() : "未知错误");
        emit receiveError(errorMsg);
    } else if (m_mode == Sending) {
        errorMsg = QString("发送文件时发生错误: %1").arg(m_tcpSocket ? m_tcpSocket->errorString() : "未知错误");
        emit sendError(errorMsg);
    }
    
    qWarning() << errorMsg;
    cleanup();
}

void FileTransferWorker::cleanup()
{
    if (m_file) {
        if (m_file->isOpen()) {
            m_file->close();
        }
        m_file->deleteLater();
        m_file = nullptr;
    }
    
    if (m_tcpSocket) {
        m_tcpSocket->disconnect();
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }
    
    if (m_tcpServer) {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
    }
    
    m_mode = None;
    m_filePath.clear();
    m_fileSize = 0;
    m_bytesTransferred = 0;
    m_expectedSize = 0;
}

