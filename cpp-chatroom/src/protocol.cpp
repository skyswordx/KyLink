#include "protocol.h"
#include <QStringList>

namespace IPMsg {

QByteArray formatMessage(quint32 packetNo, const QString& sender, 
                         const QString& host, quint32 command, 
                         const QString& extraMsg) {
    QString msg = QString("%1:%2:%3:%4:%5:%6")
        .arg(IPMSG_VERSION)
        .arg(packetNo)
        .arg(sender)
        .arg(host)
        .arg(command)
        .arg(extraMsg);
    return msg.toUtf8();
}

MessagePacket parseMessage(const QByteArray& data) {
    MessagePacket packet;
    QByteArray dataCopy = data;
    
    int colonCount = dataCopy.count(':');
    if (colonCount < 4) {
        return packet; // 返回空包表示解析失败
    }
    
    QList<QByteArray> parts = dataCopy.split(':');
    if (parts.size() < 5) {
        return packet;
    }
    
    bool ok;
    packet.version = QString::fromUtf8(parts[0]);
    packet.packetNo = parts[1].toUInt(&ok);
    if (!ok) return packet;
    
    packet.sender = QString::fromUtf8(parts[2]);
    packet.host = QString::fromUtf8(parts[3]);
    packet.command = parts[4].toUInt(&ok);
    if (!ok) return packet;
    
    if (parts.size() > 5) {
        packet.extraMsg = QString::fromUtf8(parts[5]);
    }
    
    // 解析 display_name 和 group_name (非文件传输消息)
    quint32 mode = getMode(packet.command);
    if (mode != IPMSG_SEND_FILE_REQUEST && mode != IPMSG_RECV_FILE_READY) {
        QStringList extraParts = packet.extraMsg.split('\0');
        if (extraParts.size() > 0) {
            packet.displayName = extraParts[0];
        }
        if (extraParts.size() > 1 && !extraParts[1].isEmpty()) {
            packet.groupName = extraParts[1];
        } else {
            packet.groupName = "我的好友";
        }
    }
    
    return packet;
}

quint32 getMode(quint32 command) {
    return command & IPMSG_MODE_MASK;
}

quint32 getOptions(quint32 command) {
    return command & IPMSG_OPTION_MASK;
}

} // namespace IPMsg

