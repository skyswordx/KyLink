#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMap>
#include <QString>
#include "NetworkManager.h"
#include "protocol.h"

class MainWindow;
class FileTransferWorker;
class QThread;

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(const QString& ownUsername, 
                       const IPMsg::MessagePacket& targetUserInfo,
                       const QString& targetIp,
                       NetworkManager* networkManager,
                       MainWindow* mainWindow,
                       QWidget* parent = nullptr);
    ~ChatWindow();

    void appendMessage(const QString& text, const QString& senderName, bool isOwn = false);
    void handleFileRequest(const IPMsg::MessagePacket& msg, const QString& senderIp);
    void handleFileReady(const IPMsg::MessagePacket& msg, const QString& senderIp);
    void setPendingFile(quint32 packetNo, const QString& filePath);
    void setCurrentRequestPacketNo(quint32 packetNo);

signals:
    void sendFileRequest(const QString& targetIp, const QString& filePath);
    void sendFileReady(quint16 tcpPort, quint32 packetNo, const QString& targetIp);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onSendClicked();
    void onFileClicked();
    void onReceiverReady(quint16 tcpPort, const QString& savePath);
    void onReceiveFinished(const QString& filePath);
    void onReceiveError(const QString& error);
    void onSendFinished();
    void onSendError(const QString& error);
    void onReceiveProgress(qint64 bytesReceived, qint64 totalBytes);
    void onSendProgress(qint64 bytesSent, qint64 totalBytes);

private:
    void setupUI();
    QString formatTextForDisplay(const QString& text);

    QString m_ownUsername;
    IPMsg::MessagePacket m_targetUserInfo;
    QString m_targetIp;
    NetworkManager* m_networkManager;
    MainWindow* m_mainWindow;
    
    QTextEdit* m_messageDisplay;
    QTextEdit* m_messageInput;
    QPushButton* m_sendButton;
    QPushButton* m_fileButton;
    
    QMap<quint32, QString> m_pendingFiles;  // packet_no -> filepath
    quint32 m_currentRequestPacketNo;        // 当前接收请求的包ID
    
    // 文件传输相关
    QThread* m_receiverThread;
    QThread* m_senderThread;
    FileTransferWorker* m_receiverWorker;
    FileTransferWorker* m_senderWorker;
};

#endif // CHATWINDOW_H

