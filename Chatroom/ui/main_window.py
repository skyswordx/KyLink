import socket
from PyQt5.QtWidgets import QMainWindow, QWidget, QVBoxLayout, QListWidget, QListWidgetItem, QPushButton, QLineEdit, QLabel
from PyQt5.QtCore import pyqtSlot, Qt
from core.network import NetworkCore
from ui.chat_window import ChatWindow

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PyFeiQ")
        self.setGeometry(200, 200, 300, 500)
        
        # 用户数据存储
        self.users = {} # key: ip, value: user_info_dict
        # 聊天窗口存储
        self.chat_windows = {} # key: ip, value: ChatWindow instance

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
        
        self.user_list_widget.itemDoubleClicked.connect(self.open_chat_window_from_list)
        self.search_input.textChanged.connect(self.filter_user_list)

    def closeEvent(self, event):
        """
        重写关闭事件，确保网络线程和所有聊天窗口被正确关闭
        """
        for chat_win in self.chat_windows.values():
            chat_win.close()
        self.network_thread.stop()
        event.accept()

    def update_user_list(self):
        """
        根据 self.users 字典刷新UI列表
        """
        current_selection_ip = None
        if self.user_list_widget.currentItem():
            current_selection_ip = self.user_list_widget.currentItem().data(Qt.UserRole)
            
        self.user_list_widget.clear()
        
        # 对用户按用户名排序
        sorted_users = sorted(self.users.items(), key=lambda item: item[1]['sender'])

        for ip, user_info in sorted_users:
            display_text = f"{user_info['sender']} ({ip})"
            item = QListWidgetItem(display_text)
            item.setData(Qt.UserRole, ip) # 将IP地址作为数据附加到列表项
            self.user_list_widget.addItem(item)
            
            if ip == current_selection_ip:
                self.user_list_widget.setCurrentItem(item)

        self.status_label.setText(f"在线用户: {len(self.users)}")
        self.filter_user_list() # 刷新后应用当前搜索过滤器

    @pyqtSlot(str)
    def filter_user_list(self, text=""):
        """
        根据搜索框内容过滤用户列表
        """
        search_text = self.search_input.text().lower()
        for i in range(self.user_list_widget.count()):
            item = self.user_list_widget.item(i)
            item_text = item.text().lower()
            if search_text in item_text:
                item.setHidden(False)
            else:
                item.setHidden(True)

    @pyqtSlot(dict, str)
    def handle_user_online(self, msg, ip):
        """
        处理用户上线的槽函数
        """
        # 使用gethostbyaddr尝试获取更精确的主机名，如果失败则使用原始主机名
        try:
            hostname, _, _ = socket.gethostbyaddr(ip)
            msg['host'] = hostname
        except socket.herror:
            pass # 保持原始主机名

        if ip not in self.users or self.users[ip]['sender'] != msg['sender']:
            print(f"用户上线或更新: {msg['sender']} @ {ip}")
            self.users[ip] = msg
            self.update_user_list()

    @pyqtSlot(dict, str)
    def handle_user_offline(self, msg, ip):
        """
        处理用户下线的槽函数
        """
        if ip in self.users:
            print(f"用户下线: {self.users[ip]['sender']} @ {ip}")
            # 如果有和该用户的聊天窗口，则关闭它
            if ip in self.chat_windows:
                self.chat_windows[ip].close()
                del self.chat_windows[ip]
                
            del self.users[ip]
            self.update_user_list()
            
    @pyqtSlot(dict, str)
    def handle_message_received(self, msg, ip):
        """
        处理接收到新消息的槽函数
        """
        print(f"收到来自 {msg['sender']} 的消息: {msg['extra_msg']}")
        
        # 检查聊天窗口是否存在并可见
        if ip not in self.chat_windows or not self.chat_windows[ip].isVisible():
            # 如果不存在或已关闭，则创建一个新的
            self.open_chat_window(ip)

        # 将消息追加到对应的聊天窗口
        if ip in self.chat_windows:
            self.chat_windows[ip].append_message(msg['extra_msg'], msg['sender'])
            self.chat_windows[ip].activateWindow() # 激活窗口以提示用户

    def open_chat_window(self, target_ip):
        """
        打开或激活一个聊天窗口
        """
        if target_ip not in self.users:
            print(f"错误: 尝试与未知用户 {target_ip} 聊天。")
            return

        if target_ip in self.chat_windows and self.chat_windows[target_ip].isVisible():
             # 如果窗口已存在且可见，则激活它
            self.chat_windows[target_ip].activateWindow()
        else:
            # 否则，创建一个新窗口
            target_user_info = self.users[target_ip]
            chat_win = ChatWindow(self.username, target_user_info, target_ip, self.network_thread)
            
            # 当聊天窗口关闭时，从字典中移除它
            chat_win.destroyed.connect(lambda: self.chat_windows.pop(target_ip, None))

            self.chat_windows[target_ip] = chat_win
            chat_win.show()

    @pyqtSlot(QListWidgetItem)
    def open_chat_window_from_list(self, item):
        """
        从用户列表中双击打开聊天窗口的槽函数
        """
        target_ip = item.data(Qt.UserRole)
        if target_ip:
            self.open_chat_window(target_ip)

