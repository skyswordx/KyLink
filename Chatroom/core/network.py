import socket
import time
from PyQt5.QtCore import QThread, pyqtSignal
from . import protocol

class NetworkCore(QThread):
    """
    网络核心线程，处理所有UDP/TCP通信
    """
    # 定义信号
    # pyqtSignal(dict) 中的 dict 将包含解析后的消息
    message_received = pyqtSignal(dict, str) # 消息字典, 发送方IP
    user_online = pyqtSignal(dict, str)      # 用户信息, IP
    user_offline = pyqtSignal(dict, str)     # 用户信息, IP

    def __init__(self, username, hostname, port=protocol.IPMSG_DEFAULT_PORT):
        super().__init__()
        self.username = username
        self.hostname = hostname
        self.port = port
        self.running = True
        
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # 设置广播权限
        self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        # 设置超时以便可以检查 self.running 状态
        self.udp_socket.settimeout(1.0)

    def run(self):
        """
        线程主循环，监听UDP端口
        """
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
                continue # 超时后继续循环，检查 self.running
            except Exception as e:
                print(f"接收数据时发生错误: {e}")

        self.broadcast_exit()
        self.udp_socket.close()
        print("网络核心已停止。")

    def stop(self):
        """
        停止线程
        """
        self.running = False
        self.wait()

    def process_datagram(self, data, addr):
        """
        处理收到的UDP数据包
        """
        msg = protocol.parse_message(data)
        if not msg:
            return
            
        command = msg['command']
        mode = command & protocol.IPMSG_MODE_MASK

        print(f"收到来自 {addr[0]} 的消息: {msg}")

        # 根据不同命令触发不同信号
        if mode == protocol.IPMSG_BR_ENTRY:
            # 用户上线
            self.user_online.emit(msg, addr[0])
            # 回应对方我的在线状态
            self.answer_entry(addr[0], self.port)
        elif mode == protocol.IPMSG_ANSENTRY:
            # 收到对方的上线回应
            self.user_online.emit(msg, addr[0])
        elif mode == protocol.IPMSG_BR_EXIT:
            # 用户下线
            self.user_offline.emit(msg, addr[0])
        elif mode == protocol.IPMSG_SENDMSG:
            # 收到消息
            self.message_received.emit(msg, addr[0])
            # 实现 SENDCHECKOPT 的回执
            if msg['command'] & protocol.IPMSG_SENDCHECKOPT:
                 self.send_receipt(addr[0], self.port, msg['packet_no'])


    def get_packet_no(self):
        """
        生成一个基于当前时间的包ID
        """
        return int(time.time())

    def _send_udp_message(self, command, extra_msg, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        """
        发送UDP消息的内部函数
        """
        packet_no = self.get_packet_no()
        message = protocol.format_message(
            packet_no, self.username, self.hostname, command, extra_msg
        )
        self.udp_socket.sendto(message, (dest_ip, dest_port))

    def broadcast_entry(self):
        """
        广播上线消息
        """
        print("广播上线消息...")
        self._send_udp_message(protocol.IPMSG_BR_ENTRY, self.username, '<broadcast>')
        
    def answer_entry(self, dest_ip, dest_port):
        """
        回应上线消息
        """
        print(f"向 {dest_ip} 回应上线状态...")
        self._send_udp_message(protocol.IPMSG_ANSENTRY, self.username, dest_ip, dest_port)

    def broadcast_exit(self):
        """
        广播下线消息
        """
        print("广播下线消息...")
        self._send_udp_message(protocol.IPMSG_BR_EXIT, self.username, '<broadcast>')
        
    def send_message(self, message_text, dest_ip, dest_port=protocol.IPMSG_DEFAULT_PORT):
        """
        发送聊天消息
        """
        command = protocol.IPMSG_SENDMSG | protocol.IPMSG_SENDCHECKOPT
        self._send_udp_message(command, message_text, dest_ip, dest_port)
    
    def send_receipt(self, dest_ip, dest_port, packet_no_to_confirm):
        """
        发送消息收到的回执
        """
        self._send_udp_message(protocol.IPMSG_RECVMSG, str(packet_no_to_confirm), dest_ip, dest_port)
