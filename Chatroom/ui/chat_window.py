import os
import re
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QTextEdit, QPushButton, QToolBar
from PyQt5.QtCore import pyqtSlot, Qt, QUrl
from PyQt5.QtGui import QDesktopServices, QTextCursor, QTextImageFormat
from utils.emoji_manager import EmojiManager
from ui.components.emoji_picker import EmojiPicker
from ui.components.screenshot_tool import ScreenshotTool
# --- 步骤 1: 导入我们新的自定义浏览器 ---
from ui.components.animated_text_browser import AnimatedTextBrowser

class ChatWindow(QWidget):
    """
    聊天窗口类
    """
    def __init__(self, own_username, target_user_info, target_ip, network_core, main_window):
        super().__init__()
        
        self.setWindowFlags(self.windowFlags() | Qt.Window)
        
        self.main_window = main_window
        
        self.own_username = own_username
        self.target_user_info = target_user_info
        self.target_ip = target_ip
        self.network_core = network_core
        self.screenshot_tool = None
        
        # EmojiManager现在只是一个简单的数据提供者
        self.emoji_manager = EmojiManager()
        
        self.setWindowTitle(f"与 {self.target_user_info['sender']} 聊天中")
        self.setGeometry(300, 300, 500, 400)
        
        self.init_ui()
        
    def init_ui(self):
        """
        初始化UI组件
        """
        layout = QVBoxLayout(self)
        
        # --- 步骤 2: 使用我们自己的 AnimatedTextBrowser ---
        # 它天生就知道如何处理表情动画
        self.message_display = AnimatedTextBrowser(self)
        self.message_display.setOpenExternalLinks(False)
        self.message_display.anchorClicked.connect(self.handle_link_clicked)
        
        # --- 步骤 3: 不再需要下面这些复杂的设置 ---
        # self.emoji_manager.set_target_widget(self.message_display)
        # self.emoji_manager.render_emojis("", self.message_display.document())

        toolbar = QToolBar()
        self.emoji_button = QPushButton("表情")
        self.screenshot_button = QPushButton("截图")
        toolbar.addWidget(self.emoji_button)
        toolbar.addWidget(self.screenshot_button)

        self.message_input = QTextEdit()
        self.message_input.setFixedHeight(100)
        
        button_layout = QHBoxLayout()
        close_button = QPushButton("关闭")
        send_button = QPushButton("发送")
        button_layout.addStretch(1)
        button_layout.addWidget(close_button)
        button_layout.addWidget(send_button)
        
        layout.addWidget(self.message_display)
        layout.addWidget(toolbar)
        layout.addWidget(self.message_input)
        layout.addLayout(button_layout)
        
        send_button.clicked.connect(self.send_message)
        close_button.clicked.connect(self.close)
        self.emoji_button.clicked.connect(self.open_emoji_picker)
        self.screenshot_button.clicked.connect(self.start_screenshot)

    def format_text_for_display(self, text):
        """
        格式化文本，将纯文本中的emoji代码转换为HTML的<img>标签
        """
        text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

        def replace_emoji(match):
            code = match.group(0)
            # 使用标准的 emoji:// 协议头
            return f'<img src="emoji://{code}" />'

        formatted_text = re.sub(r'\[\d+\]', replace_emoji, text)
        return formatted_text.replace("\n", "<br>")

    @pyqtSlot()
    def send_message(self):
        """
        发送消息的槽函数
        """
        message_text = self.message_input.toPlainText()
        if not message_text.strip():
            return
            
        self.append_message(message_text, self.own_username, is_own=True)
        self.network_core.send_message(message_text, self.target_ip)
        self.message_input.clear()

    def append_message(self, text, sender_name, is_own=False):
        """
        向消息显示区域追加一条消息
        """
        if is_own:
            header = f'<p style="color: green;"><b>{sender_name} (我):</b></p>'
        else:
            header = f'<p style="color: blue;"><b>{sender_name}:</b></p>'
        
        self.message_display.append(header)
        
        formatted_content = self.format_text_for_display(text)
        self.message_display.insertHtml(formatted_content)
        self.message_display.append("")

        self.message_display.ensureCursorVisible()

    def append_image(self, image_path, sender_name, is_own=False):
        """
        在聊天窗口中显示一张本地图片（如截图）
        """
        if is_own:
            header = f'<p style="color: green;"><b>{sender_name} (我) 发送了截图:</b></p>'
        else:
            header = f'<p style="color: blue;"><b>{sender_name} 发送了截图:</b></p>'
            
        self.message_display.append(header)
        
        image_url = QUrl.fromLocalFile(os.path.abspath(image_path))
        cursor = self.message_display.textCursor()
        image_format = QTextImageFormat()
        image_format.setName(image_url.toString())
        cursor.insertImage(image_format)
        
        self.message_display.append("")
        self.message_display.ensureCursorVisible()

    @pyqtSlot()
    def open_emoji_picker(self):
        """
        打开表情选择器
        """
        picker = EmojiPicker(self.emoji_manager, self)
        picker.emoji_selected.connect(self.insert_emoji_code)
        button_pos = self.emoji_button.mapToGlobal(self.emoji_button.pos())
        picker.move(button_pos.x() - picker.width(), button_pos.y() - picker.height())
        picker.exec_()
        
    @pyqtSlot(str)
    def insert_emoji_code(self, code):
        """
        将表情代码插入到输入框
        """
        self.message_input.insertPlainText(code)

    @pyqtSlot()
    def start_screenshot(self):
        """
        开始截图
        """
        self.main_window.hide()
        self.hide()
        
        self.screenshot_tool = ScreenshotTool()
        self.screenshot_tool.screenshot_taken.connect(self.handle_screenshot_taken)
        self.screenshot_tool.show()

    @pyqtSlot(str)
    def handle_screenshot_taken(self, image_path):
        """
        处理截图完成后的信号
        """
        self.show()
        self.main_window.show()
        self.activateWindow()

        self.append_image(image_path, self.own_username, is_own=True)
        
        message_to_send = f"[截图: {os.path.basename(image_path)}]"
        self.network_core.send_message(message_to_send, self.target_ip)
        
        self.screenshot_tool = None

    def handle_link_clicked(self, url):
        """
        如果用户点击了 file:// 链接，则尝试用系统默认方式打开
        """
        if url.scheme() == 'file':
            QDesktopServices.openUrl(url)

    def closeEvent(self, event):
        print(f"关闭与 {self.target_ip} 的聊天窗口")
        # 从主窗口的字典中移除自己
        if self.target_ip in self.main_window.chat_windows:
            del self.main_window.chat_windows[self.target_ip]
        super().closeEvent(event)

