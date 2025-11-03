import socket
import os
from PyQt5.QtCore import QThread, pyqtSignal
from . import protocol

class FileServerThread(QThread):
    """
    标准IPMSG协议文件服务器线程
    监听2425端口，处理文件传输请求
    """
    file_request_received = pyqtSignal(int, int, int, str)  # packet_no, file_id, offset, client_ip
    
    def __init__(self, port=protocol.IPMSG_DEFAULT_PORT):
        super().__init__()
        self.port = port
        self.running = True
        self.server_socket = None
        
    def run(self):
        """启动TCP文件服务器"""
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind(('', self.port))
            self.server_socket.listen(5)
            self.server_socket.settimeout(1.0)
            print(f"文件服务器已启动，监听端口 {self.port}")
            
            while self.running:
                try:
                    conn, addr = self.server_socket.accept()
                    # 处理连接
                    self._handle_client(conn, addr[0])
                except socket.timeout:
                    continue
                except Exception as e:
                    if self.running:
                        print(f"文件服务器接受连接错误: {e}")
                        
        except OSError as e:
            print(f"文件服务器无法启动: {e}")
        finally:
            if self.server_socket:
                self.server_socket.close()
                
    def _handle_client(self, conn, client_ip):
        """处理客户端连接，解析IPMSG_GETFILEDATA请求"""
        try:
            # 读取请求数据（标准IPMSG格式：{packetNo(hex)}:{fileId(hex)}:{offset(hex)}:）
            data = conn.recv(1024)
            if not data:
                conn.close()
                return
                
            request_str = data.decode('utf-8', errors='ignore').strip()
            parts = request_str.split(':')
            
            if len(parts) >= 3:
                packet_no = int(parts[0], 16)
                file_id = int(parts[1], 16)
                offset = int(parts[2], 16) if parts[2] else 0
                
                # 发射信号，通知主线程处理文件传输
                self.file_request_received.emit(packet_no, file_id, offset, client_ip)
                
                # 连接保持打开，等待主线程发送文件数据
                # 注意：实际的文件传输应该在主线程中处理
                # 这里只是解析请求，实际的发送逻辑应该在FileSender中
                
        except (ValueError, IndexError) as e:
            print(f"解析文件请求失败: {e}")
            conn.close()
        except Exception as e:
            print(f"处理客户端连接错误: {e}")
            conn.close()
            
    def stop(self):
        """停止服务器"""
        self.running = False
        if self.server_socket:
            self.server_socket.close()
        self.wait()

