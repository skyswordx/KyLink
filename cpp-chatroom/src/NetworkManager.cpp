#include "NetworkManager.h"
#include <QNetworkInterface>
#include <QDateTime>
#include <QDebug>
#include <QByteArray>

NetworkManager::NetworkManager(const QString& username, const QString& hostname, 
                               const QString& groupname, quint16 port, QObject* parent)
    : QObject(parent)
    , m_username(username)
    , m_hostname(hostname)
    , m_groupname(groupname)
    , m_port(port)
    , m_udpSocket(nullptr)
    , m_broadcastTimer(nullptr)
    , m_running(false)
{
    m_udpSocket = new QUdpSocket(this);
    m_broadcastTimer = new QTimer(this);
    
    connect(m_broadcastTimer, &QTimer::timeout, this, &NetworkManager::broadcastEntry);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onReadyRead);
}

NetworkManager::~NetworkManager()
{
    stop();
}

bool NetworkManager::start()
{
    if (m_running) {
        return true;
    }
    
    if (!m_udpSocket->bind(m_port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        QString error = QString("无法绑定端口 %1: %2")
            .arg(m_port)
            .arg(m_udpSocket->errorString());
        qWarning() << error;
        emit errorOccurred(error);
        return false;
    }
    
    m_running = true;
    qDebug() << QString("网络核心已启动，监听端口 %1").arg(m_port);
    
    // 立即广播一次上线消息
    broadcastEntry();
    
    // 每30秒广播一次心跳
    m_broadcastTimer->start(30000);
    
    return true;
}

void NetworkManager::stop()
{
    if (!m_running) {
        return;
    }
    
    m_running = false;
    m_broadcastTimer->stop();
    
    // 广播下线消息
    QString payload = m_username;
    sendUdpMessage(IPMsg::IPMSG_BR_EXIT, payload, "<broadcast>", IPMsg::IPMSG_DEFAULT_PORT);
    
    m_udpSocket->close();
    qDebug() << "网络核心已停止。";
}

void NetworkManager::onReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        QHostAddress sender;
        quint16 port;
        
        datagram.resize(static_cast<int>(m_udpSocket->pendingDatagramSize()));
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &port);
        
        if (!datagram.isEmpty()) {
            processDatagram(datagram, sender, port);
        }
    }
}

void NetworkManager::processDatagram(const QByteArray& data, const QHostAddress& sender, quint16 port)
{
    IPMsg::MessagePacket msg = IPMsg::parseMessage(data);
    if (msg.packetNo == 0 && msg.command == 0) {
        // 解析失败
        return;
    }
    
    QString senderIp = sender.toString();
    quint32 mode = IPMsg::getMode(msg.command);
    
    qDebug() << QString("收到来自 %1 的消息: command=%2").arg(senderIp).arg(msg.command);
    
    if (mode == IPMsg::IPMSG_BR_ENTRY) {
        emit userOnline(msg, senderIp);
        answerEntry(senderIp, port);
    } else if (mode == IPMsg::IPMSG_ANSENTRY) {
        emit userOnline(msg, senderIp);
    } else if (mode == IPMsg::IPMSG_BR_EXIT) {
        emit userOffline(msg, senderIp);
    } else if (mode == IPMsg::IPMSG_SENDMSG) {
        emit messageReceived(msg, senderIp);
        if (msg.command & IPMsg::IPMSG_SENDCHECKOPT) {
            sendReceipt(senderIp, port, msg.packetNo);
        }
    } else if (mode == IPMsg::IPMSG_SEND_FILE_REQUEST) {
        emit fileRequestReceived(msg, senderIp);
    } else if (mode == IPMsg::IPMSG_RECV_FILE_READY) {
        emit fileReceiverReady(msg, senderIp);
    }
}

void NetworkManager::sendUdpMessage(quint32 command, const QString& extraMsgPayload, 
                                    const QString& destIp, quint16 destPort)
{
    quint32 packetNo = getPacketNo();
    QByteArray message = IPMsg::formatMessage(packetNo, m_username, m_hostname, 
                                              command, extraMsgPayload);
    
    if (destIp == "<broadcast>") {
        // 广播到所有网络接口
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& iface : interfaces) {
            if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
                iface.flags().testFlag(QNetworkInterface::IsRunning) &&
                !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
                
                QList<QNetworkAddressEntry> entries = iface.addressEntries();
                for (const QNetworkAddressEntry& entry : entries) {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                        QHostAddress broadcastAddr = entry.broadcast();
                        if (!broadcastAddr.isNull()) {
                            m_udpSocket->writeDatagram(message, broadcastAddr, destPort);
                        }
                    }
                }
            }
        }
        // 也发送到本地广播地址
        m_udpSocket->writeDatagram(message, QHostAddress::Broadcast, destPort);
    } else {
        m_udpSocket->writeDatagram(message, QHostAddress(destIp), destPort);
    }
}

void NetworkManager::broadcastEntry()
{
    qDebug() << "广播上线消息...";
    // 使用QByteArray构造包含null字符的payload
    QByteArray payloadBytes = m_username.toUtf8();
    payloadBytes.append('\0');
    payloadBytes.append(m_groupname.toUtf8());
    QString payload = QString::fromUtf8(payloadBytes);
    sendUdpMessage(IPMsg::IPMSG_BR_ENTRY, payload, "<broadcast>", IPMsg::IPMSG_DEFAULT_PORT);
}

void NetworkManager::answerEntry(const QString& destIp, quint16 destPort)
{
    qDebug() << QString("向 %1 回应上线状态...").arg(destIp);
    // 使用QByteArray构造包含null字符的payload
    QByteArray payloadBytes = m_username.toUtf8();
    payloadBytes.append('\0');
    payloadBytes.append(m_groupname.toUtf8());
    QString payload = QString::fromUtf8(payloadBytes);
    sendUdpMessage(IPMsg::IPMSG_ANSENTRY, payload, destIp, destPort);
}

void NetworkManager::sendMessage(const QString& messageText, const QString& destIp, quint16 destPort)
{
    quint32 command = IPMsg::IPMSG_SENDMSG | IPMsg::IPMSG_SENDCHECKOPT;
    sendUdpMessage(command, messageText, destIp, destPort);
}

void NetworkManager::sendReceipt(const QString& destIp, quint16 destPort, quint32 packetNoToConfirm)
{
    sendUdpMessage(IPMsg::IPMSG_RECVMSG, QString::number(packetNoToConfirm), destIp, destPort);
}

void NetworkManager::sendFileRequest(const QString& filename, qint64 filesize, 
                                     const QString& destIp, quint16 destPort)
{
    QString payload = QString("%1:%2").arg(filename).arg(filesize);
    sendUdpMessage(IPMsg::IPMSG_SEND_FILE_REQUEST, payload, destIp, destPort);
}

void NetworkManager::sendFileReadySignal(quint16 tcpPort, quint32 packetNo, 
                                         const QString& destIp, quint16 destPort)
{
    QString payload = QString("%1:%2").arg(tcpPort).arg(packetNo);
    sendUdpMessage(IPMsg::IPMSG_RECV_FILE_READY, payload, destIp, destPort);
}

quint32 NetworkManager::getPacketNo() const
{
    return static_cast<quint32>(QDateTime::currentMSecsSinceEpoch());
}

