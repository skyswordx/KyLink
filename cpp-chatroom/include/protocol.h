#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>
#include <QByteArray>

// FeiQ (IP Messenger) Protocol Constants
// 参考 Python 版本中的 protocol.py

namespace IPMsg {

// --- 基本信息 ---
constexpr char IPMSG_VERSION = '1';
constexpr quint16 IPMSG_DEFAULT_PORT = 2425;

// --- 命令常量 ---
// 命令的主模式
constexpr quint32 IPMSG_MODE_MASK = 0x000000FF;
constexpr quint32 IPMSG_BROADCAST = 0x00000001;
constexpr quint32 IPMSG_MULTICAST = 0x00000002;

// 命令的选项
constexpr quint32 IPMSG_OPTION_MASK = 0xFFFFFF00;
constexpr quint32 IPMSG_SENDCHECKOPT = 0x00000100;     // 传送检查 (需要对方返回 RECVMSG)
constexpr quint32 IPMSG_SECRETOPT = 0x00000200;        // 加密选项 (此项目中暂不实现)
constexpr quint32 IPMSG_BROADCASTOPT = 0x00001000;     // 广播
constexpr quint32 IPMSG_MULTICASTOPT = 0x00002000;     // 多播
constexpr quint32 IPMSG_NOPOPUPOPT = 0x00004000;       // 不自动弹出窗口
constexpr quint32 IPMSG_AUTORETOPT = 0x00008000;       // 自动回复
constexpr quint32 IPMSG_RETRYOPT = 0x00010000;         // 重试
constexpr quint32 IPMSG_PASSWORDOPT = 0x00020000;      // 密码选项
constexpr quint32 IPMSG_NOLOGOPT = 0x00200000;         // 不记录日志

// --- 核心命令 ---
// 上线
constexpr quint32 IPMSG_BR_ENTRY = 0x00000001;
// 下线
constexpr quint32 IPMSG_BR_EXIT = 0x00000002;
// 回应上线
constexpr quint32 IPMSG_ANSENTRY = 0x00000003;
// 保持在线 (心跳)
constexpr quint32 IPMSG_BR_ABSENCE = 0x00000004;

// 发送消息
constexpr quint32 IPMSG_SENDMSG = 0x00000020;
// 收到消息回执
constexpr quint32 IPMSG_RECVMSG = 0x00000021;
// 读取消息 (用于群聊)
constexpr quint32 IPMSG_READMSG = 0x00000030;
// 删除消息 (用于群聊)
constexpr quint32 IPMSG_DELMSG = 0x00000031;

// --- 文件传输相关命令 ---
// 请求发送文件
constexpr quint32 IPMSG_SEND_FILE_REQUEST = 0x00000060;
// 接收方准备就绪，可以开始发送
constexpr quint32 IPMSG_RECV_FILE_READY = 0x00000061;

// --- 消息结构 ---
struct MessagePacket {
    QString version;
    quint32 packetNo;
    QString sender;
    QString host;
    quint32 command;
    QString extraMsg;
    QString displayName;  // 解析后提取
    QString groupName;    // 解析后提取
};

// --- 消息格式化函数 ---
QByteArray formatMessage(quint32 packetNo, const QString& sender, 
                         const QString& host, quint32 command, 
                         const QString& extraMsg);

// --- 消息解析函数 ---
MessagePacket parseMessage(const QByteArray& data);

// --- 辅助函数 ---
quint32 getMode(quint32 command);
quint32 getOptions(quint32 command);

} // namespace IPMsg

#endif // PROTOCOL_H

