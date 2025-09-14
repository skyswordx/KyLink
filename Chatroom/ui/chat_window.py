import os
import re
import time
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QTextEdit, QPushButton, QToolBar, QMessageBox
from PyQt5.QtCore import pyqtSlot, Qt, QUrl, pyqtSignal # Added pyqtSignal here
from PyQt5.QtGui import QDesktopServices, QTextCursor, QTextImageFormat
from utils.emoji_manager import EmojiManager
from ui.components.emoji_picker import EmojiPicker
from ui.components.screenshot_tool import ScreenshotTool
from ui.components.animated_text_browser import AnimatedTextBrowser
from core.file_transfer import FileSender, FileReceiver # 导入文件传输类

class ChatWindow(QWidget):
    """
    聊天窗口類
    """
    # 信号：(目标IP, 文件路径)
    send_file_request = pyqtSignal(str, str)
    # 信号：(TCP端口, 包ID, 目标IP)
    send_file_ready = pyqtSignal(int, int, str)

    def __init__(self, own_username, target_user_info, target_ip, network_core, main_window):
        super().__init__()
        
        self.setWindowFlags(self.windowFlags() | Qt.Window)
        
        self.main_window = main_window
        self.own_username = own_username
        self.target_user_info = target_user_info
        self.target_ip = target_ip
        self.network_core = network_core
        self.screenshot_tool = None
        self.emoji_manager = EmojiManager()
        
        # 存储待发送的文件信息 {packet_no: filepath}
        self.pending_files = {}

        self.setWindowTitle(f"與 {self.target_user_info['sender']} 聊天中")
        self.setGeometry(300, 300, 500, 400)
        
        self.init_ui()
        
    def init_ui(self):
        layout = QVBoxLayout(self)
        self.message_display = AnimatedTextBrowser(self)
        self.message_display.setOpenExternalLinks(False)
        self.message_display.anchorClicked.connect(self.handle_link_clicked)
        toolbar = QToolBar()
        self.emoji_button = QPushButton("表情")
        self.screenshot_button = QPushButton("截圖")
        toolbar.addWidget(self.emoji_button)
        toolbar.addWidget(self.screenshot_button)
        self.message_input = QTextEdit()
        self.message_input.setFixedHeight(100)
        button_layout = QHBoxLayout()
        close_button = QPushButton("關閉")
        send_button = QPushButton("發送")
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
        text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        def replace_emoji(match):
            code = match.group(0)
            return f'<img src="emoji:{code}" />'
        formatted_text = re.sub(r'\[\d+\]', replace_emoji, text)
        return formatted_text.replace("\n", "<br>")

    @pyqtSlot()
    def send_message(self):
        message_text = self.message_input.toPlainText()
        if not message_text.strip():
            return
        self.append_message(message_text, self.own_username, is_own=True)
        self.network_core.send_message(message_text, self.target_ip)
        self.message_input.clear()

    def append_message(self, text, sender_name, is_own=False):
        if is_own:
            sender_html = f'<p style="color: green; margin-bottom: 0;"><b>{sender_name} (我):</b></p>'
        else:
            sender_html = f'<p style="color: blue; margin-bottom: 0;"><b>{sender_name}:</b></p>'
        content_html = self.format_text_for_display(text)
        full_html = f'{sender_html}<div style="margin-left: 10px;">{content_html}</div>'
        print(f"[append_message] 正在向聊天視窗新增以下 HTML: {full_html}")
        self.message_display.append(full_html)

    def append_image(self, image_path, sender_name, is_own=False):
        if is_own:
            header = f'<p style="color: green;"><b>{sender_name} (我) 發送了截圖:</b></p>'
        else:
            header = f'<p style="color: blue;"><b>{sender_name} 發送了截圖:</b></p>'
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
        picker = EmojiPicker(self.emoji_manager, self)
        picker.emoji_selected.connect(self.insert_emoji_code)
        button_pos = self.emoji_button.mapToGlobal(self.emoji_button.pos())
        picker.move(button_pos.x() - picker.width(), button_pos.y() - picker.height())
        picker.exec_()
        
    @pyqtSlot(str)
    def insert_emoji_code(self, code):
        self.message_input.insertPlainText(code)

    @pyqtSlot()
    def start_screenshot(self):
        self.main_window.hide()
        self.hide()
        self.screenshot_tool = ScreenshotTool()
        self.screenshot_tool.screenshot_taken.connect(self.handle_screenshot_taken)
        self.screenshot_tool.show()

    @pyqtSlot(str)
    def handle_screenshot_taken(self, image_path):
        self.show()
        self.main_window.show()
        self.activateWindow()
        self.append_image(image_path, self.own_username, is_own=True)
        
        # --- 修改：不再发送文字，而是触发文件发送请求 ---
        self.send_file_request.emit(self.target_ip, image_path)

    # --- 新增：处理文件请求和响应 ---
    @pyqtSlot(dict, str)
    def handle_file_request(self, msg, sender_ip):
        """处理收到的文件传输请求"""
        parts = msg['extra_msg'].split(':')
        filename, filesize = parts[0], parts[1]
        
        reply = QMessageBox.question(self, '文件傳輸請求', 
            f"用戶 {self.target_user_info['sender']} ({sender_ip}) 想傳送檔案:\n"
            f"名稱: {filename}\n"
            f"大小: {filesize} bytes\n\n您是否同意接收？",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No)

        if reply == QMessageBox.Yes:
            self.receiver_thread = FileReceiver("cache", filename, filesize)
            # 连接信号，当接收线程准备好后，发送UDP就绪信号
            self.receiver_thread.ready_to_receive.connect(
                lambda port, save_path: self.send_file_ready.emit(port, msg['packet_no'], sender_ip)
            )
            # 文件接收完成后，在界面上显示图片
            self.receiver_thread.transfer_finished.connect(
                lambda path: self.append_image(path, self.target_user_info['sender'], is_own=False)
            )
            self.receiver_thread.start()

    @pyqtSlot(dict, str)
    def handle_file_ready(self, msg, sender_ip):
        """处理对方已准备好接收文件的信号"""
        parts = msg['extra_msg'].split(':')
        tcp_port, original_packet_no = parts[0], int(parts[1])
        
        # 从待发送文件列表中找到对应的文件路径
        filepath = self.pending_files.get(original_packet_no)
        if filepath:
            print(f"對方已準備就緒，開始透過TCP傳送檔案 {filepath} 到 {sender_ip}:{tcp_port}")
            self.sender_thread = FileSender(sender_ip, tcp_port, filepath)
            self.sender_thread.start()
            # 可以在这里连接 finished 和 error 信号来更新UI
            self.pending_files.pop(original_packet_no) # 发送后从列表中移除

    def handle_link_clicked(self, url):
        if url.scheme() == 'file':
            QDesktopServices.openUrl(url)

    def closeEvent(self, event):
        print(f"關閉與 {self.target_ip} 的聊天窗口")
        if self.target_ip in self.main_window.chat_windows:
            del self.main_window.chat_windows[self.target_ip]
        super().closeEvent(event)