# FeiQ (IP Messenger) Protocol Constants
# 严格遵照 IPMSG 协议标准，参考飞鸽传书/飞秋实现
# 参考 C++ 版本中的 feiqlib/protocol.h 和 feiqlib/ipmsg.h

# --- 基本信息 ---
IPMSG_VERSION = b'1'
IPMSG_DEFAULT_PORT = 2425

# --- 命令常量 ---
# 命令的主模式
IPMSG_MODE_MASK = 0x000000FF
IPMSG_BROADCAST = 0x00000001
IPMSG_MULTICAST = 0x00000002

# 命令的选项（option for all command）
IPMSG_ABSENCEOPT = 0x00000100
IPMSG_SERVEROPT = 0x00000200
IPMSG_DIALUPOPT = 0x00010000
IPMSG_FILEATTACHOPT = 0x00200000  # 文件附加选项
IPMSG_ENCRYPTOPT = 0x00400000
IPMSG_UTF8OPT = 0x00800000

# 命令的选项（option for send command）
IPMSG_OPTION_MASK = 0xFFFFFF00
IPMSG_SENDCHECKOPT = 0x00000100     # 传送检查 (需要对方返回 RECVMSG)
IPMSG_SECRETOPT = 0x00000200        # 加密选项 (此项目中暂不实现)
IPMSG_BROADCASTOPT = 0x00000400     # 广播
IPMSG_MULTICASTOPT = 0x00000800     # 多播
IPMSG_NOPOPUPOPT = 0x00001000       # 不自动弹出窗口
IPMSG_AUTORETOPT = 0x00002000       # 自动回复
IPMSG_RETRYOPT = 0x00004000         # 重试
IPMSG_PASSWORDOPT = 0x00008000      # 密码选项
IPMSG_NOLOGOPT = 0x00020000         # 不记录日志
IPMSG_NOADDLISTOPT = 0x00080000     # 不添加到列表
IPMSG_READCHECKOPT = 0x00100000     # 已读检查

# --- 核心命令 ---
# 上线/下线
IPMSG_BR_ENTRY = 0x00000001         # 用户上线
IPMSG_BR_EXIT = 0x00000002          # 用户下线
IPMSG_ANSENTRY = 0x00000003         # 回应上线
IPMSG_BR_ABSENCE = 0x00000004       # 保持在线 (心跳)

# 消息相关
IPMSG_SENDMSG = 0x00000020          # 发送消息
IPMSG_RECVMSG = 0x00000021          # 收到消息回执
IPMSG_READMSG = 0x00000030          # 读取消息 (用于群聊)
IPMSG_DELMSG = 0x00000031           # 删除消息 (用于群聊)

# 文件传输相关命令（标准IPMSG协议）
IPMSG_GETFILEDATA = 0x00000060      # 请求文件数据
IPMSG_RELEASEFILES = 0x00000061     # 释放文件
IPMSG_GETDIRFILES = 0x00000062     # 请求目录文件

# --- 文件类型 ---
IPMSG_FILE_REGULAR = 0x00000001     # 普通文件
IPMSG_FILE_DIR = 0x00000002         # 目录

# --- 分隔符常量 ---
FILELIST_SEPARATOR = b'\x07'        # ASCII 7，文件列表分隔符
HLIST_ENTRY_SEPARATOR = b':'        # 主机列表项分隔符（即冒号）
HOSTLIST_DUMMY = b'\x08'            # ASCII 8，主机列表占位符

# --- 辅助函数 ---
def IS_CMD_SET(cmd, test):
    """检查命令是否匹配"""
    return (cmd & 0xFF) == test

def IS_OPT_SET(cmd, opt):
    """检查选项是否设置"""
    return (cmd & opt) == opt


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
        extra_msg.encode('utf-8') if isinstance(extra_msg, str) else extra_msg
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
            'extra_msg': parts[5] if len(parts) > 5 else b''  # 保持为bytes，因为可能包含二进制数据
        }
        return msg_dict
    except (ValueError, IndexError):
        # 解析失败
        return None

def parse_file_attach(extra_msg_bytes):
    """
    解析标准IPMSG文件附加格式
    extra_msg格式: "{text_message}\0{fileId}:{filename}:{filesize(hex)}:{modifyTime(hex)}:{fileType(hex)}:{FILELIST_SEPARATOR}"
    
    返回: (text_message, file_list)
    file_list: [(file_id, filename, filesize, modify_time, file_type), ...]
    """
    if isinstance(extra_msg_bytes, str):
        extra_bytes = extra_msg_bytes.encode('utf-8')
    else:
        extra_bytes = extra_msg_bytes
    
    # 查找第一个\0分隔符
    null_pos = extra_bytes.find(b'\0')
    if null_pos == -1:
        # 没有文件，只有文本消息
        return extra_bytes.decode('utf-8', errors='ignore'), []
    
    text_message = extra_bytes[:null_pos].decode('utf-8', errors='ignore')
    file_part = extra_bytes[null_pos + 1:]
    
    file_list = []
    
    # 查找文件列表分隔符 FILELIST_SEPARATOR (0x07)
    start = 0
    while start < len(file_part):
        sep_pos = file_part.find(FILELIST_SEPARATOR, start)
        if sep_pos == -1:
            # 最后一个文件，没有分隔符
            file_info = file_part[start:]
            if file_info:
                file_list.append(_parse_single_file(file_info))
            break
        
        file_info = file_part[start:sep_pos]
        if file_info:
            file_list.append(_parse_single_file(file_info))
        
        start = sep_pos + 1
    
    return text_message, file_list

def _parse_single_file(file_info_bytes):
    """
    解析单个文件信息
    格式: {fileId}:{filename}:{filesize(hex)}:{modifyTime(hex)}:{fileType(hex)}
    """
    try:
        parts = file_info_bytes.split(HLIST_ENTRY_SEPARATOR)
        if len(parts) < 5:
            return None
        
        file_id = int(parts[0])
        filename = parts[1].decode('utf-8', errors='ignore').replace('::', ':')  # 恢复冒号
        filesize = int(parts[2], 16)  # 十六进制
        modify_time = int(parts[3], 16) if parts[3] else 0
        file_type = int(parts[4], 16) if parts[4] else IPMSG_FILE_REGULAR
        
        return {
            'file_id': file_id,
            'filename': filename,
            'filesize': filesize,
            'modify_time': modify_time,
            'file_type': file_type
        }
    except (ValueError, IndexError) as e:
        print(f"解析文件信息失败: {e}")
        return None