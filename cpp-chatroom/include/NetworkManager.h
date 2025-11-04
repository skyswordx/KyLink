#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>
#include "protocol.h"

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(const QString& username, const QString& hostname, 
                           const QString& groupname = "我的好友", 
                           quint16 port = IPMsg::IPMSG_DEFAULT_PORT,
                           QObject* parent = nullptr);
    ~NetworkManager();

    bool start();
    void stop();
    
    // 发送消息
    void sendMessage(const QString& messageText, const QString& destIp, 
                    quint16 destPort = IPMsg::IPMSG_DEFAULT_PORT);
    
    // 文件传输相关
    void sendFileRequest(const QString& filename, qint64 filesize, 
                        const QString& destIp, 
                        quint16 destPort = IPMsg::IPMSG_DEFAULT_PORT);
    void sendFileReadySignal(quint16 tcpPort, quint32 packetNo, 
                            const QString& destIp, 
                            quint16 destPort = IPMsg::IPMSG_DEFAULT_PORT);
    
    // 获取包编号
    quint32 getPacketNo() const;

signals:
    // 用户上线/下线
    void userOnline(const IPMsg::MessagePacket& msg, const QString& ip);
    void userOffline(const IPMsg::MessagePacket& msg, const QString& ip);
    
    // 消息接收
    void messageReceived(const IPMsg::MessagePacket& msg, const QString& ip);
    
    // 文件传输
    void fileRequestReceived(const IPMsg::MessagePacket& msg, const QString& ip);
    void fileReceiverReady(const IPMsg::MessagePacket& msg, const QString& ip);
    
    // 错误信号
    void errorOccurred(const QString& error);

private slots:
    void onReadyRead();
    void broadcastEntry();

private:
    void processDatagram(const QByteArray& data, const QHostAddress& sender, quint16 port);
    void sendUdpMessage(quint32 command, const QString& extraMsgPayload, 
                       const QString& destIp, quint16 destPort);
    void answerEntry(const QString& destIp, quint16 destPort);
    void sendReceipt(const QString& destIp, quint16 destPort, quint32 packetNoToConfirm);

private:
    QString m_username;
    QString m_hostname;
    QString m_groupname;
    quint16 m_port;
    
    QUdpSocket* m_udpSocket;
    QTimer* m_broadcastTimer;
    bool m_running;
};

#endif // NETWORKMANAGER_H

