# FeiQ (IP Messenger) Protocol Constants
# 参考 C++ 版本中的 feiqlib/protocol.h 和 feiqlib/ipmsg.h

# --- 基本信息 ---
IPMSG_VERSION = b'1'
IPMSG_DEFAULT_PORT = 2425

# --- 命令常量 ---
# 命令的主模式
IPMSG_MODE_MASK = 0x000000FF
IPMSG_BROADCAST = 0x00000001
IPMSG_MULTICAST = 0x00000002

# 命令的选项
IPMSG_OPTION_MASK = 0xFFFFFF00
IPMSG_SENDCHECKOPT = 0x00000100     # 传送检查 (需要对方返回 RECVMSG)
IPMSG_SECRETOPT = 0x00000200        # 加密选项 (此项目中暂不实现)
IPMSG_BROADCASTOPT = 0x00001000     # 广播
IPMSG_MULTICASTOPT = 0x00002000     # 多播
IPMSG_NOPOPUPOPT = 0x00004000       # 不自动弹出窗口
IPMSG_AUTORETOPT = 0x00008000       # 自动回复
IPMSG_RETRYOPT = 0x00010000         # 重试
IPMSG_PASSWORDOPT = 0x00020000      # 密码选项
IPMSG_NOLOGOPT = 0x00200000         # 不记录日志

# --- 核心命令 ---
# 上线
IPMSG_BR_ENTRY = 0x00000001
# 下线
IPMSG_BR_EXIT = 0x00000002
# 回应上线
IPMSG_ANSENTRY = 0x00000003
# 保持在线 (心跳)
IPMSG_BR_ABSENCE = 0x00000004

# 发送消息
IPMSG_SENDMSG = 0x00000020
# 收到消息回执
IPMSG_RECVMSG = 0x00000021
# 读取消息 (用于群聊)
IPMSG_READMSG = 0x00000030
# 删除消息 (用于群聊)
IPMSG_DELMSG = 0x00000031

# --- 消息格式化 ---
# 消息格式: "版本号:包编号:发送者:发送主机:命令:附加信息"
# 例如: b"1:12345:user1:host1:32:Hello"

def format_message(packet_no, sender, host, command, extra_msg):
    """
    格式化要发送的消息包
    """
    return b":".join([
        IPMSG_VERSION,
        str(packet_no).encode('utf-8'),
        sender.encode('utf-8'),
        host.encode('utf-8'),
        str(command).encode('utf-8'),
        extra_msg.encode('utf-8')
    ])

def parse_message(data):
    """
    解析收到的消息包
    返回一个包含消息各部分的字典
    """
    parts = data.split(b':', 5)
    if len(parts) < 5:
        return None
    
    try:
        msg_dict = {
            'version': parts[0].decode('utf-8', errors='ignore'),
            'packet_no': int(parts[1]),
            'sender': parts[2].decode('utf-8', errors='ignore'),
            'host': parts[3].decode('utf-8', errors='ignore'),
            'command': int(parts[4]),
            'extra_msg': parts[5].decode('utf-8', errors='ignore') if len(parts) > 5 else ''
        }
        return msg_dict
    except (ValueError, IndexError):
        # 解析失败
        return None
