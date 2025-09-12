import socket
from PyQt5.QtWidgets import QMainWindow, QWidget, QVBoxLayout, QListWidget, QPushButton, QLineEdit, QLabel
from PyQt5.QtCore import pyqtSlot
from core.network import NetworkCore

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PyFeiQ")
        self.setGeometry(200, 200, 300, 500)
        
        # 用户数据存储
        self.users = {} # key: ip, value: user_info_dict

        # --- UI 控件 ---
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("搜索用户、IP...")
        
        self.user_list_widget = QListWidget()
        
        self.status_label = QLabel("正在初始化...")
        
        layout.addWidget(self.search_input)
        layout.addWidget(self.user_list_widget)
        layout.addWidget(self.status_label)
        
        # --- 获取本机信息 ---
        self.username = "User" # 可从配置文件读取
        self.hostname = socket.gethostname()
        
        # --- 初始化网络核心 ---
        self.network_thread = NetworkCore(self.username, self.hostname)
        self.setup_connections()
        self.network_thread.start()

    def setup_connections(self):
        """
        设置信号和槽的连接
        """
        self.network_thread.user_online.connect(self.handle_user_online)
        self.network_thread.user_offline.connect(self.handle_user_offline)
        self.network_thread.message_received.connect(self.handle_message_received)
        self.user_list_widget.itemDoubleClicked.connect(self.open_chat_window)

    def closeEvent(self, event):
        """
        重写关闭事件，确保网络线程被正确关闭
        """
        self.network_thread.stop()
        event.accept()

    def update_user_list(self):
        """
        根据 self.users 字典刷新UI列表
        """
        self.user_list_widget.clear()
        for ip, user_info in self.users.items():
            display_text = f"{user_info['sender']} ({ip})"
            self.user_list_widget.addItem(display_text)
        self.status_label.setText(f"在线用户: {len(self.users)}")

    @pyqtSlot(dict, str)
    def handle_user_online(self, msg, ip):
        """
        处理用户上线的槽函数
        """
        if ip not in self.users:
            print(f"新用户上线: {msg['sender']} @ {ip}")
            self.users[ip] = msg
            self.update_user_list()

    @pyqtSlot(dict, str)
    def handle_user_offline(self, msg, ip):
        """
        处理用户下线的槽函数
        """
        if ip in self.users:
            print(f"用户下线: {self.users[ip]['sender']} @ {ip}")
            del self.users[ip]
            self.update_user_list()
            
    @pyqtSlot(dict, str)
    def handle_message_received(self, msg, ip):
        """
        处理接收到新消息的槽函数
        """
        # TODO: 
        # 1. 检查是否已经有和该用户的聊天窗口
        # 2. 如果有，将消息追加到窗口
        # 3. 如果没有，创建一个新的聊天窗口
        # 4. 在用户列表中给该用户一个提示 (例如，闪烁或改变颜色)
        print(f"收到来自 {msg['sender']} 的消息: {msg['extra_msg']}")
        # 临时的模拟，实际应打开聊天窗口
        self.statusBar().showMessage(f"收到来自 {msg['sender']} 的新消息!", 5000)

    @pyqtSlot()
    def open_chat_window(self):
        """
        双击用户列表项时打开聊天窗口
        """
        selected_item = self.user_list_widget.currentItem()
        if not selected_item:
            return
            
        # 这里需要从 item 的文本中解析出 IP 地址
        # 这是一个简化的示例，实际应用中最好将 IP 直接关联到 ListWidgetItem 上
        # 例如: item.setData(Qt.UserRole, ip)
        # item_text = selected_item.text() -> "user (192.168.1.10)"
        
        print(f"准备打开与 {selected_item.text()} 的聊天窗口...")
        # TODO:
        # 1. 创建 ChatWindow 实例
        # 2. 将目标用户的 IP 和信息传递给聊天窗口
        # 3. 显示聊天窗口
        pass
