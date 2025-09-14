import socket
import os
from PyQt5.QtCore import QThread, pyqtSignal

class FileReceiver(QThread):
    """
    文件接收线程 (TCP Server)
    """
    # 信号：(TCP端口号, 文件保存路径)
    ready_to_receive = pyqtSignal(int, str)
    # 信号：(文件保存路径)
    transfer_finished = pyqtSignal(str)
    # 信号：(错误信息)
    transfer_error = pyqtSignal(str)

    def __init__(self, save_dir, filename, filesize):
        super().__init__()
        self.save_dir = save_dir
        self.filename = filename
        self.filesize = int(filesize)
        self.running = True

    def run(self):
        try:
            # 创建TCP socket并绑定到0号端口，由系统自动选择一个可用端口
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.bind(('', 0))
            port = self.server_socket.getsockname()[1]
            self.server_socket.listen(1)

            save_path = os.path.join(self.save_dir, self.filename)
            
            # 发射信号，告知主线程已准备就绪，并传递端口号和保存路径
            self.ready_to_receive.emit(port, save_path)
            
            # 等待发送方连接，设置超时
            self.server_socket.settimeout(20) # 20秒超时
            conn, addr = self.server_socket.accept()
            
            received_size = 0
            with open(save_path, 'wb') as f:
                while received_size < self.filesize:
                    data = conn.recv(4096)
                    if not data:
                        break
                    f.write(data)
                    received_size += len(data)
            
            conn.close()
            self.server_socket.close()

            if received_size == self.filesize:
                self.transfer_finished.emit(save_path)
            else:
                self.transfer_error.emit(f"文件接收不完整: {self.filename}")

        except socket.timeout:
            self.transfer_error.emit("等待文件发送方连接超时。")
        except Exception as e:
            self.transfer_error.emit(f"文件接收失败: {e}")
            
    def stop(self):
        self.running = False
        if hasattr(self, 'server_socket'):
            self.server_socket.close()
        self.quit()

class FileSender(QThread):
    """
    文件发送线程 (TCP Client)
    """
    transfer_finished = pyqtSignal()
    transfer_error = pyqtSignal(str)

    def __init__(self, host, port, filepath):
        super().__init__()
        self.host = host
        self.port = int(port)
        self.filepath = filepath

    def run(self):
        try:
            client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client_socket.connect((self.host, self.port))
            
            with open(self.filepath, 'rb') as f:
                while True:
                    data = f.read(4096)
                    if not data:
                        break
                    client_socket.sendall(data)
            
            client_socket.close()
            self.transfer_finished.emit()
            
        except Exception as e:
            self.transfer_error.emit(f"文件发送失败: {e}")