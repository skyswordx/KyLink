#ifndef FILETRANSFERWORKER_H
#define FILETRANSFERWORKER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QThread>

class FileTransferWorker : public QObject
{
    Q_OBJECT

public:
    explicit FileTransferWorker(QObject* parent = nullptr);
    ~FileTransferWorker();

    // 文件接收模式
    void startReceiving(const QString& savePath, qint64 expectedSize);
    
    // 文件发送模式
    void startSending(const QString& filePath, const QString& host, quint16 port);

signals:
    // 接收模式信号
    void readyToReceive(quint16 tcpPort, const QString& savePath);
    void receiveFinished(const QString& filePath);
    void receiveError(const QString& error);
    void receiveProgress(qint64 bytesReceived, qint64 totalBytes);
    
    // 发送模式信号
    void sendFinished();
    void sendError(const QString& error);
    void sendProgress(qint64 bytesSent, qint64 totalBytes);

private slots:
    void onNewConnection();
    void onClientConnected();
    void onClientReadyRead();
    void onClientBytesWritten(qint64 bytes);
    void onSocketError(QAbstractSocket::SocketError error);

private:
    enum TransferMode {
        None,
        Receiving,
        Sending
    };
    
    TransferMode m_mode;
    QTcpServer* m_tcpServer;
    QTcpSocket* m_tcpSocket;
    QFile* m_file;
    QString m_filePath;
    qint64 m_fileSize;
    qint64 m_bytesTransferred;
    qint64 m_expectedSize;
    
    void cleanup();
    void setupReceiving();
    void setupSending();
};

#endif // FILETRANSFERWORKER_H

