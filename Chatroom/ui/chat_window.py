import os
import re
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QTextBrowser, QTextEdit, QPushButton, QToolBar
from PyQt5.QtCore import pyqtSlot, Qt, QUrl
from PyQt5.QtGui import QDesktopServices, QTextCursor, QTextImageFormat
from utils.emoji_manager import EmojiManager
from ui.components.emoji_picker import EmojiPicker
from ui.components.screenshot_tool import ScreenshotTool

class ChatWindow(QWidget):
    """
    聊天窗口类
    """
    def __init__(self, own_username, target_user_info, target_ip, network_core, main_window):
        # --- 关键修复 1: 调用 super().__init__() 时不传递父对象 ---
        # 这会强制Qt将此控件创建一个独立的、顶级的窗口，而不是子控件。
        super().__init__()
        
        # 我们仍然保存对主窗口的引用，以便将来调用它的功能（例如截图时隐藏它）
        self.main_window = main_window
        
        self.own_username = own_username
        self.target_user_info = target_user_info
        self.target_ip = target_ip
        self.network_core = network_core
        self.screenshot_tool = None
        
        self.emoji_manager = EmojiManager()
        
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
        self.message_display.setOpenExternalLinks(False) # 自己处理链接点击
        self.message_display.anchorClicked.connect(self.handle_link_clicked)
        # 将emoji渲染器关联到QTextBrowser的document
        self.emoji_manager.render_emojis("", self.message_display.document())


        # 功能工具栏
        toolbar = QToolBar()
        self.emoji_button = QPushButton("表情")
        self.screenshot_button = QPushButton("截图")
        toolbar.addWidget(self.emoji_button)
        toolbar.addWidget(self.screenshot_button)

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
        self.emoji_button.clicked.connect(self.open_emoji_picker)
        self.screenshot_button.clicked.connect(self.start_screenshot)

    def format_text_for_display(self, text):
        """
        格式化文本，将纯文本中的emoji代码转换为HTML的<img>标签
        """
        # HTML转义，防止消息内容被当作HTML解析
        text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

        def replace_emoji(match):
            code = match.group(0)
            # 使用前面注册的 "emoji://" 协议
            return f'<img src="emoji://{code}" />'

        # 使用正则表达式匹配如 `[01]` `[123]` 这样的代码
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
        
        # 插入格式化后的消息内容
        formatted_content = self.format_text_for_display(text)
        self.message_display.insertHtml(formatted_content)
        self.message_display.append("") # 添加一个空行以分隔消息

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
        
        # 将本地文件路径转换为URL格式
        image_url = QUrl.fromLocalFile(os.path.abspath(image_path))
        cursor = self.message_display.textCursor()
        image_format = QTextImageFormat()
        image_format.setName(image_url.toString())
        # 可以设置图片大小
        # image_format.setWidth(200)
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
        # 将选择器定位在按钮旁边
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
        # --- 关键修复 2: 使用 self.main_window 引用来隐藏主窗口 ---
        # 因为我们不再是子控件，self.parent() 会是 None。
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
        # 恢复窗口显示
        self.show()
        self.main_window.show()
        self.activateWindow()

        # 在本地显示截图
        self.append_image(image_path, self.own_username, is_own=True)
        
        # TODO: 发送截图文件
        # 目前，我们只发送一个文本通知，因为文件传输需要更复杂的TCP协议
        message_to_send = f"[截图: {os.path.basename(image_path)}]"
        self.network_core.send_message(message_to_send, self.target_ip)
        
        self.screenshot_tool = None # 清理引用


    def handle_link_clicked(self, url):
        """
        如果用户点击了 file:// 链接，则尝试用系统默认方式打开
        """
        if url.scheme() == 'file':
            QDesktopServices.openUrl(url)
        # 其他协议可以继续在这里添加处理

    def closeEvent(self, event):
        print(f"关闭与 {self.target_ip} 的聊天窗口")
        event.accept()
