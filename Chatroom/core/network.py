import socket
import time
from PyQt5.QtCore import QThread, pyqtSignal
from . import protocol

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
        self.port = port
        self.running = True
        
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.udp_socket.settimeout(1.0)

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
        print("网络核心已停止。")

    def stop(self):
        self.running = False
        self.wait()

    def process_datagram(self, data, addr):
        msg = protocol.parse_message(data)
        if not msg:
            return
            
        command = msg['command']
        mode = command & protocol.IPMSG_MODE_MASK
        
        # 仅在非文件传输消息中解析display_name和group_name
        if mode not in [protocol.IPMSG_SEND_FILE_REQUEST, protocol.IPMSG_RECV_FILE_READY]:
            extra_parts = msg['extra_msg'].split('\0', 1)
            msg['display_name'] = extra_parts[0]
            msg['group_name'] = extra_parts[1] if len(extra_parts) > 1 and extra_parts[1] else "我的好友"

        print(f"收到来自 {addr[0]} 的消息: {msg}")

        if mode == protocol.IPMSG_BR_ENTRY:
            self.user_online.emit(msg, addr[0])
            self.answer_entry(addr[0], self.port)
        elif mode == protocol.IPMSG_ANSENTRY:
            self.user_online.emit(msg, addr[0])
        elif mode == protocol.IPMSG_BR_EXIT:
            self.user_offline.emit(msg, addr[0])
        elif mode == protocol.IPMSG_SENDMSG:
            self.message_received.emit(msg, addr[0])
            if msg['command'] & protocol.IPMSG_SENDCHECKOPT:
                 self.send_receipt(addr[0], self.port, msg['packet_no'])
        # --- 新增文件传输指令处理 ---
        elif mode == protocol.IPMSG_SEND_FILE_REQUEST:
            self.file_request_received.emit(msg, addr[0])
        elif mode == protocol.IPMSG_RECV_FILE_READY:
            self.file_receiver_ready.emit(msg, addr[0])
            
    def get_packet_no(self):
        return int(time.time())

    def _send_udp_message(self, command, extra_msg_payload, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        packet_no = self.get_packet_no()
        message = protocol.format_message(
            packet_no, self.username, self.hostname, command, extra_msg_payload
        )
        self.udp_socket.sendto(message, (dest_ip, dest_port))

    def broadcast_entry(self):
        print("广播上线消息...")
        payload = f"{self.username}\0{self.groupname}"
        self._send_udp_message(protocol.IPMSG_BR_ENTRY, payload, '<broadcast>')
        
    def answer_entry(self, dest_ip, dest_port):
        print(f"向 {dest_ip} 回应上线状态...")
        payload = f"{self.username}\0{self.groupname}"
        self._send_udp_message(protocol.IPMSG_ANSENTRY, payload, dest_ip, dest_port)

    def broadcast_exit(self):
        print("广播下线消息...")
        self._send_udp_message(protocol.IPMSG_BR_EXIT, self.username, '<broadcast>')
        
    def send_message(self, message_text, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        command = protocol.IPMSG_SENDMSG | protocol.IPMSG_SENDCHECKOPT
        self._send_udp_message(command, message_text, dest_ip, dest_port)
    
    def send_receipt(self, dest_ip, dest_port, packet_no_to_confirm):
        self._send_udp_message(protocol.IPMSG_RECVMSG, str(packet_no_to_confirm), dest_ip, dest_port)

    # --- 新增文件传输方法 ---
    def send_file_request(self, filename, filesize, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        """
        发送文件传输请求
        :param filename: 文件名
        :param filesize: 文件大小
        """
        payload = f"{filename}:{filesize}"
        self._send_udp_message(protocol.IPMSG_SEND_FILE_REQUEST, payload, dest_ip, dest_port)
    
    def send_file_ready_signal(self, tcp_port, packet_no, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        """
        告知对方已准备好接收文件
        :param tcp_port: 本地监听的TCP端口
        :param packet_no: 原始请求的包ID，用于对方识别
        """
        payload = f"{tcp_port}:{packet_no}"
        self._send_udp_message(protocol.IPMSG_RECV_FILE_READY, payload, dest_ip, dest_port)