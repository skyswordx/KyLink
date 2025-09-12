from PyQt5.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QTextBrowser, QTextEdit, QPushButton, QToolBar
from PyQt5.QtCore import pyqtSlot, Qt

class ChatWindow(QWidget):
    """
    聊天窗口类
    """
    def __init__(self, own_username, target_user_info, target_ip, network_core, parent=None):
        super().__init__(parent)
        
        self.own_username = own_username
        self.target_user_info = target_user_info
        self.target_ip = target_ip
        self.network_core = network_core
        
        self.setWindowTitle(f"与 {self.target_user_info['sender']} 聊天中")
        self.setGeometry(300, 300, 500, 400)
        
        self.init_ui()
        
    def init_ui(self):
        """
        初始化UI组件
        """
        layout = QVBoxLayout(self)
        
        # 消息显示区域
        self.message_display = QTextBrowser()
        self.message_display.setOpenExternalLinks(True) # 允许打开链接

        # 功能工具栏
        toolbar = QToolBar()
        # TODO: 添加表情、截图等按钮
        emoji_button = QPushButton("表情")
        screenshot_button = QPushButton("截图")
        toolbar.addWidget(emoji_button)
        toolbar.addWidget(screenshot_button)

        # 消息输入区域
        self.message_input = QTextEdit()
        self.message_input.setFixedHeight(100)
        
        # 发送按钮区域
        button_layout = QHBoxLayout()
        close_button = QPushButton("关闭")
        send_button = QPushButton("发送")
        button_layout.addStretch(1)
        button_layout.addWidget(close_button)
        button_layout.addWidget(send_button)
        
        # 组装布局
        layout.addWidget(self.message_display)
        layout.addWidget(toolbar)
        layout.addWidget(self.message_input)
        layout.addLayout(button_layout)
        
        # --- 连接信号与槽 ---
        send_button.clicked.connect(self.send_message)
        close_button.clicked.connect(self.close)

    @pyqtSlot()
    def send_message(self):
        """
        发送消息的槽函数
        """
        message_text = self.message_input.toPlainText()
        if not message_text.strip():
            return # 不发送空消息
            
        # 在本地显示自己发送的消息
        self.append_message(message_text, self.own_username, is_own=True)
        
        # 通过网络核心发送消息
        self.network_core.send_message(message_text, self.target_ip)
        
        # 清空输入框
        self.message_input.clear()

    def append_message(self, text, sender_name, is_own=False):
        """
        向消息显示区域追加一条消息
        """
        # 使用HTML格式化消息，便于设置颜色和样式
        if is_own:
            # 自己发送的消息用绿色显示
            formatted_message = f'<p style="color: green;"><b>{sender_name} (我):</b></p>'
        else:
            # 对方发送的消息用蓝色显示
            formatted_message = f'<p style="color: blue;"><b>{sender_name}:</b></p>'
        
        self.message_display.append(formatted_message)
        # 使用 insertPlainText 来避免 text 被当成HTML解析，防止XSS
        self.message_display.insertPlainText(text + "\n")
        
        # 滚动到底部
        self.message_display.ensureCursorVisible()

    def closeEvent(self, event):
        """
        窗口关闭事件
        """
        # 在父窗口(MainWindow)中移除对这个窗口的引用
        # 这里我们发送一个信号或者让MainWindow自己处理
        print(f"关闭与 {self.target_ip} 的聊天窗口")
        event.accept()
