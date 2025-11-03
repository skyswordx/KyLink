import socket
import time
import threading
from PyQt5.QtCore import QThread, pyqtSignal, QObject
from . import protocol

# 文件ID生成器（简单实现，使用时间戳+计数器）
class FileIdGenerator:
    _instance = None
    _counter = 0
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance
    
    def generate(self):
        """生成唯一的文件ID"""
        self._counter += 1
        return int(time.time() * 1000) + self._counter

file_id_gen = FileIdGenerator()

# 文件传输管理器（存储待发送的文件信息）
class FileTransferManager(QObject):
    """管理待发送的文件"""
    _instance = None
    _files = {}  # {(packet_no, file_id): {'path': ..., 'filename': ..., 'filesize': ...}}
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance
    
    def register_file(self, packet_no, file_id, filepath, filename, filesize):
        """注册待发送的文件"""
        self._files[(packet_no, file_id)] = {
            'path': filepath,
            'filename': filename,
            'filesize': filesize
        }
    
    def get_file(self, packet_no, file_id):
        """获取文件信息"""
        return self._files.get((packet_no, file_id))
    
    def remove_file(self, packet_no, file_id):
        """移除文件信息"""
        self._files.pop((packet_no, file_id), None)

file_transfer_mgr = FileTransferManager()

def handle_tcp_file_request(conn, addr, packet_no, file_id, offset):
    """处理TCP文件传输请求（标准IPMSG协议）"""
    try:
        file_info = file_transfer_mgr.get_file(packet_no, file_id)
        if not file_info:
            conn.close()
            return
        
        filepath = file_info['path']
        filesize = file_info['filesize']
        
        # 发送文件数据
        with open(filepath, 'rb') as f:
            if offset > 0:
                f.seek(offset)
            
            sent_size = offset
            while sent_size < filesize:
                data = f.read(4096)
                if not data:
                    break
                conn.sendall(data)
                sent_size += len(data)
        
        conn.close()
        file_transfer_mgr.remove_file(packet_no, file_id)
        print(f"文件发送完成: {filepath} -> {addr[0]}")
        
    except Exception as e:
        print(f"文件发送错误: {e}")
        conn.close()

def tcp_file_server_thread(port, running_flag):
    """TCP文件服务器线程（标准IPMSG协议，监听2425端口）"""
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server_socket.bind(('', port))
        server_socket.listen(5)
        server_socket.settimeout(1.0)
        print(f"TCP文件服务器已启动，监听端口 {port}")
        
        while running_flag[0]:
            try:
                conn, addr = server_socket.accept()
                
                # 解析请求（标准IPMSG格式：{packetNo(hex)}:{fileId(hex)}:{offset(hex)}:）
                data = conn.recv(1024)
                if not data:
                    conn.close()
                    continue
                
                request_str = data.decode('utf-8', errors='ignore').strip()
                parts = request_str.split(':')
                
                if len(parts) >= 3:
                    packet_no = int(parts[0], 16)
                    file_id = int(parts[1], 16)
                    offset = int(parts[2], 16) if parts[2] else 0
                    
                    # 在单独线程中处理文件传输
                    thread = threading.Thread(
                        target=handle_tcp_file_request,
                        args=(conn, addr, packet_no, file_id, offset)
                    )
                    thread.daemon = True
                    thread.start()
                else:
                    conn.close()
                    
            except socket.timeout:
                continue
            except Exception as e:
                if running_flag[0]:
                    print(f"TCP文件服务器错误: {e}")
                    
    except OSError as e:
        print(f"TCP文件服务器无法启动: {e}")
    finally:
        server_socket.close()

class NetworkCore(QThread):
    """
    网络核心线程，处理所有UDP/TCP通信
    """
    # 定义信号
    message_received = pyqtSignal(dict, str) # 消息字典, 发送方IP
    user_online = pyqtSignal(dict, str)      # 用户信息, IP
    user_offline = pyqtSignal(dict, str)     # 用户信息, IP

    # 新增文件传输相关信号
    # pyqtSignal(dict: 消息包, str: 发送方IP)
    file_request_received = pyqtSignal(dict, str)
    # pyqtSignal(dict: 消息包, str: 原始请求方IP)
    file_receiver_ready = pyqtSignal(dict, str)

    def __init__(self, username, hostname, groupname="我的好友", port=protocol.IPMSG_DEFAULT_PORT):
        super().__init__()
        self.username = username
        self.hostname = hostname
        self.groupname = groupname
        self.display_name = username  # 默认使用用户名作为显示名称
        self.port = port
        self.running = True
        self.tcp_server_running = [True]  # 用于TCP服务器线程
        
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.udp_socket.settimeout(1.0)
        
        # 启动TCP文件服务器（标准IPMSG协议，监听2425端口）
        self.tcp_server_thread = threading.Thread(
            target=tcp_file_server_thread,
            args=(self.port, self.tcp_server_running),
            daemon=True
        )
        self.tcp_server_thread.start()

    def run(self):
        try:
            self.udp_socket.bind(('', self.port))
            print(f"网络核心已启动，监听端口 {self.port}")
            self.broadcast_entry()
        except OSError as e:
            print(f"错误：无法绑定端口 {self.port}。程序是否已在运行？ {e}")
            return

        while self.running:
            try:
                data, addr = self.udp_socket.recvfrom(1024)
                if data:
                    self.process_datagram(data, addr)
            except socket.timeout:
                continue
            except Exception as e:
                print(f"接收数据时发生错误: {e}")

        self.broadcast_exit()
        self.udp_socket.close()
        self.tcp_server_running[0] = False  # 停止TCP服务器
        print("网络核心已停止。")

    def stop(self):
        self.running = False
        self.wait()

    def process_datagram(self, data, addr):
        msg = protocol.parse_message(data)
        if not msg:
            return
            
        command = msg['command']
        mode = protocol.IS_CMD_SET(command, protocol.IPMSG_BR_ENTRY) or \
               protocol.IS_CMD_SET(command, protocol.IPMSG_ANSENTRY) or \
               protocol.IS_CMD_SET(command, protocol.IPMSG_BR_EXIT) or \
               protocol.IS_CMD_SET(command, protocol.IPMSG_SENDMSG) or \
               protocol.IS_CMD_SET(command, protocol.IPMSG_GETFILEDATA)
        
        if not mode:
            mode = command & protocol.IPMSG_MODE_MASK
        
        print(f"收到来自 {addr[0]} 的消息: cmd=0x{command:x}, mode={mode}")

        if protocol.IS_CMD_SET(command, protocol.IPMSG_BR_ENTRY):
            # 上线消息：extra_msg 格式为标准IPMSG只包含用户名，但飞秋扩展支持 "username\0groupname"
            extra_bytes = msg['extra_msg']
            if isinstance(extra_bytes, bytes):
                extra_str = extra_bytes.decode('utf-8', errors='ignore')
            else:
                extra_str = extra_bytes
            
            # 尝试解析扩展格式（飞秋格式）
            if '\0' in extra_str:
                extra_parts = extra_str.split('\0', 1)
                msg['display_name'] = extra_parts[0]
                msg['group_name'] = extra_parts[1] if len(extra_parts) > 1 and extra_parts[1] else "我的好友"
            else:
                # 标准IPMSG格式，只有用户名
                msg['display_name'] = extra_str
                msg['group_name'] = "我的好友"
            
            self.user_online.emit(msg, addr[0])
            self.answer_entry(addr[0], self.port)
        elif protocol.IS_CMD_SET(command, protocol.IPMSG_ANSENTRY):
            # 回应上线消息：同上处理
            extra_bytes = msg['extra_msg']
            if isinstance(extra_bytes, bytes):
                extra_str = extra_bytes.decode('utf-8', errors='ignore')
            else:
                extra_str = extra_bytes
            
            if '\0' in extra_str:
                extra_parts = extra_str.split('\0', 1)
                msg['display_name'] = extra_parts[0]
                msg['group_name'] = extra_parts[1] if len(extra_parts) > 1 and extra_parts[1] else "我的好友"
            else:
                msg['display_name'] = extra_str
                msg['group_name'] = "我的好友"
            
            self.user_online.emit(msg, addr[0])
        elif protocol.IS_CMD_SET(command, protocol.IPMSG_BR_EXIT):
            self.user_offline.emit(msg, addr[0])
        elif protocol.IS_CMD_SET(command, protocol.IPMSG_SENDMSG):
            # 文本消息或带文件的消息
            if protocol.IS_OPT_SET(command, protocol.IPMSG_FILEATTACHOPT):
                # 带文件的消息：解析标准IPMSG格式
                # extra_msg格式为 "{text}\0{fileId}:{filename}:{filesize(hex)}:{modifyTime(hex)}:{fileType(hex)}:{FILELIST_SEPARATOR}"
                text_message, file_list = protocol.parse_file_attach(msg['extra_msg'])
                if file_list:
                    # 添加解析后的文件信息到消息字典
                    msg['file_list'] = file_list
                    msg['text_message'] = text_message
                    self.file_request_received.emit(msg, addr[0])
                else:
                    # 解析失败，当作普通消息处理
                    self.message_received.emit(msg, addr[0])
            else:
                # 纯文本消息：extra_msg 就是消息内容
                if isinstance(msg['extra_msg'], bytes):
                    msg['extra_msg'] = msg['extra_msg'].decode('utf-8', errors='ignore')
                self.message_received.emit(msg, addr[0])
                if protocol.IS_OPT_SET(command, protocol.IPMSG_SENDCHECKOPT):
                    self.send_receipt(addr[0], self.port, msg['packet_no'])
        elif protocol.IS_CMD_SET(command, protocol.IPMSG_GETFILEDATA):
            # 接收方请求文件数据
            # extra_msg格式: {packetNo(hex)}:{fileId(hex)}:{offset(hex)}:
            try:
                extra_bytes = msg['extra_msg']
                if isinstance(extra_bytes, bytes):
                    extra_str = extra_bytes.decode('utf-8', errors='ignore')
                else:
                    extra_str = extra_bytes
                
                parts = extra_str.split(':')
                if len(parts) >= 3:
                    packet_no = int(parts[0], 16)
                    file_id = int(parts[1], 16)
                    offset = int(parts[2], 16) if parts[2] else 0
                    msg['file_packet_no'] = packet_no
                    msg['file_id'] = file_id
                    msg['file_offset'] = offset
                self.file_receiver_ready.emit(msg, addr[0])
            except (ValueError, IndexError) as e:
                print(f"解析IPMSG_GETFILEDATA失败: {e}")
            
    def get_packet_no(self):
        return int(time.time())
    
    def get_file_id(self):
        """获取新的文件ID"""
        return file_id_gen.generate()

    def _send_udp_message(self, command, extra_msg_payload, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        packet_no = self.get_packet_no()
        message = protocol.format_message(
            packet_no, self.username, self.hostname, command, extra_msg_payload
        )
        self.udp_socket.sendto(message, (dest_ip, dest_port))

    def broadcast_entry(self):
        print("广播上线消息...")
        # 标准IPMSG只发送用户名，但为了兼容飞秋，我们发送扩展格式
        # 飞秋可能会忽略\0后的内容，标准IPMSG只读取用户名
        # 使用display_name作为发送的用户名（如果设置了的话）
        payload_username = self.display_name if hasattr(self, 'display_name') and self.display_name else self.username
        payload = f"{payload_username}\0{self.groupname}"
        self._send_udp_message(protocol.IPMSG_BR_ENTRY, payload, '<broadcast>')
        
    def answer_entry(self, dest_ip, dest_port):
        print(f"向 {dest_ip} 回应上线状态...")
        # 同上，发送扩展格式以兼容飞秋和标准IPMSG
        payload_username = self.display_name if hasattr(self, 'display_name') and self.display_name else self.username
        payload = f"{payload_username}\0{self.groupname}"
        self._send_udp_message(protocol.IPMSG_ANSENTRY, payload, dest_ip, dest_port)

    def broadcast_exit(self):
        print("广播下线消息...")
        self._send_udp_message(protocol.IPMSG_BR_EXIT, self.username, '<broadcast>')
        
    def send_message(self, message_text, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        command = protocol.IPMSG_SENDMSG | protocol.IPMSG_SENDCHECKOPT
        self._send_udp_message(command, message_text, dest_ip, dest_port)
    
    def send_receipt(self, dest_ip, dest_port, packet_no_to_confirm):
        self._send_udp_message(protocol.IPMSG_RECVMSG, str(packet_no_to_confirm), dest_ip, dest_port)

    # --- 标准IPMSG文件传输方法 ---
    def send_file_request(self, filename, filesize, file_id, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        """
        发送文件传输请求（标准IPMSG协议）
        :param filename: 文件名
        :param filesize: 文件大小（字节）
        :param file_id: 文件ID（唯一标识）
        :param dest_ip: 目标IP
        :param dest_port: 目标端口
        """
        import os
        import time
        
        # 标准IPMSG文件传输格式：
        # extra_msg = "{text_message}\0{fileId}:{filename}:{filesize(hex)}:{modifyTime(hex)}:{fileType(hex)}:{FILELIST_SEPARATOR}"
        # 文件名中的冒号需要用::表示
        # filename参数实际可能是filepath，需要处理
        filepath = filename if os.path.isabs(filename) or os.path.exists(filename) else filename
        basename = os.path.basename(filepath)
        safe_filename = basename.replace(':', '::')
        modify_time = int(os.path.getmtime(filepath)) if os.path.exists(filepath) else int(time.time())
        file_type = protocol.IPMSG_FILE_REGULAR
        
        # 构造文件信息字符串
        file_info = f"{file_id}:{safe_filename}:{filesize:x}:{modify_time:x}:{file_type:x}{protocol.FILELIST_SEPARATOR.decode('latin1')}"
        
        # 消息格式：文本消息 + \0 + 文件信息
        # 对于图片，我们可以发送一个简单的提示文本
        text_message = f"[图片] {basename}"
        payload = f"{text_message}\0{file_info}"
        
        # 使用标准IPMSG命令：IPMSG_SENDMSG | IPMSG_FILEATTACHOPT | IPMSG_SENDCHECKOPT
        command = protocol.IPMSG_SENDMSG | protocol.IPMSG_FILEATTACHOPT | protocol.IPMSG_SENDCHECKOPT
        packet_no = self.get_packet_no()
        
        # 注册文件到文件传输管理器（供TCP服务器使用）
        file_transfer_mgr.register_file(packet_no, file_id, filepath, basename, filesize)
        
        self._send_udp_message(command, payload, dest_ip, dest_port)
        return packet_no
    
    def send_file_request_ready(self, packet_no, file_id, offset, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        """
        发送文件数据请求（标准IPMSG协议 IPMSG_GETFILEDATA）
        :param packet_no: 原始消息的包编号
        :param file_id: 文件ID
        :param offset: 文件偏移量（从0开始）
        :param dest_ip: 目标IP
        :param dest_port: 目标端口
        """
        # 标准IPMSG格式：{packetNo(hex)}:{fileId(hex)}:{offset(hex)}:
        payload = f"{packet_no:x}:{file_id:x}:{offset:x}:"
        self._send_udp_message(protocol.IPMSG_GETFILEDATA, payload, dest_ip, dest_port)