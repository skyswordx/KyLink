# Chatroom/ui/chat_window.py (已修正)

import os
import re
import time
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout
from PyQt5.QtCore import pyqtSlot, Qt, QUrl, pyqtSignal
from PyQt5.QtGui import QDesktopServices, QTextCursor, QTextImageFormat

from qframelesswindow import FramelessWindow, StandardTitleBar
from qfluentwidgets import (TextEdit, PushButton, MessageBox, ToolButton,
                            FluentIcon as FIF, isDarkTheme, SmoothScrollBar, 
                            setTheme, Theme)

from utils.emoji_manager import EmojiManager
from ui.components.emoji_picker import EmojiPicker
from ui.components.screenshot_tool import ScreenshotTool
from ui.components.animated_text_browser import AnimatedTextBrowser
from core.file_transfer import FileSender, FileReceiver

class ChatWindow(FramelessWindow):
    send_file_request = pyqtSignal(str, str)
    send_file_ready = pyqtSignal(int, int, str)

    def __init__(self, own_username, target_user_info, target_ip, network_core, main_window):
        super().__init__()
        
        self.main_window = main_window
        self.own_username = own_username
        self.target_user_info = target_user_info
        self.target_ip = target_ip
        self.network_core = network_core
        self.screenshot_tool = None
        self.emoji_manager = EmojiManager()
        self.pending_files = {}

        sender_name = self.target_user_info.get('sender', 'Unknown')
        self.setWindowTitle(f"与 {sender_name} 聊天")
        self.setGeometry(300, 300, 500, 500)
        
        self.setTitleBar(StandardTitleBar(self))
        self.titleBar.raise_()

        self.init_ui()
        
    def init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, self.titleBar.height(), 0, 0)
        self.chat_area_widget = QWidget()
        layout.addWidget(self.chat_area_widget)

        content_layout = QVBoxLayout(self.chat_area_widget)
        content_layout.setContentsMargins(15, 15, 15, 15)
        content_layout.setSpacing(10)

        # 使用支持自定义资源协议（emoji:）的 AnimatedTextBrowser，确保表情能正确显示
        self.message_display = AnimatedTextBrowser(self)
        self.message_display.setOpenExternalLinks(False)
        self.message_display.anchorClicked.connect(self.handle_link_clicked)
        
        # 为滚动区域启用 Fluent 的平滑滚动条（内部会接管原生滚动条）
        SmoothScrollBar(Qt.Vertical, self.message_display)
        self.message_display.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

        toolbar_layout = QHBoxLayout()
        self.emoji_button = ToolButton(FIF.EMOJI_TAB_SYMBOLS, self)
        self.emoji_button.setToolTip("表情")
        self.screenshot_button = ToolButton(FIF.CUT, self)
        self.screenshot_button.setToolTip("截图")
        toolbar_layout.addWidget(self.emoji_button)
        toolbar_layout.addWidget(self.screenshot_button)
        toolbar_layout.addStretch(1)

        self.message_input = TextEdit(self)
        self.message_input.setPlaceholderText("输入消息...")
        self.message_input.setFixedHeight(100)
        
        button_layout = QHBoxLayout()
        send_button = PushButton("发送")
        send_button.setIcon(FIF.SEND)
        button_layout.addStretch(1)
        button_layout.addWidget(send_button)
        
        content_layout.addWidget(self.message_display)
        content_layout.addLayout(toolbar_layout)
        content_layout.addWidget(self.message_input)
        content_layout.addLayout(button_layout)
        
        send_button.clicked.connect(self.send_message)
        self.emoji_button.clicked.connect(self.open_emoji_picker)
        self.screenshot_button.clicked.connect(self.start_screenshot)

        self.set_window_style()
        # 确保无边框标题栏文本与窗口标题同步
        if hasattr(self, 'titleBar'):
            try:
                self.titleBar.setTitle(self.windowTitle())
            except Exception:
                pass

    def set_window_style(self):
        """ 根據當前主題為視窗和其子元件設定樣式，與主視窗風格保持一致 """
        # 不改变全局主题，只在当前窗口内做最小必要的配色统一
        if isDarkTheme():
            window_bg = "rgb(32, 32, 32)"
            widget_bg = "rgb(43, 43, 43)"
            text_color = "white"
        else:
            window_bg = "rgb(243, 243, 243)"
            widget_bg = "white"
            text_color = "black"

        self.chat_area_widget.setObjectName("ChatAreaWidget")

        style_sheet = f"""
            ChatWindow {{
                background-color: {window_bg};
            }}
            #ChatAreaWidget {{
                background-color: {window_bg};
            }}
            AnimatedTextBrowser, TextEdit {{
                background-color: {widget_bg};
                color: {text_color};
                border: none;
                border-radius: 5px;
            }}
        """
        self.setStyleSheet(style_sheet)

        # 統一標題列在暗色主題下的字體與按鈕圖標顏色
        if hasattr(self, 'titleBar') and self.titleBar is not None:
            if isDarkTheme():
                titlebar_qss = """
                    /* 標題文字 */
                    QLabel { color: white; }
                    /* 標題欄上的工具按鈕（最小化/最大化/關閉）*/
                    QToolButton { color: white; }
                    QToolButton:hover { background-color: rgba(255,255,255,0.08); }
                    QToolButton:pressed { background-color: rgba(255,255,255,0.14); }
                """
                self.titleBar.setStyleSheet(titlebar_qss)
            else:
                self.titleBar.setStyleSheet("")

    # ... (文件其余部分保持不变) ...

    def format_text_for_display(self, text):
        text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        def replace_emoji(match):
            code = match.group(0)
            return f'<img src="emoji:{code}" />'
        formatted_text = re.sub(r'\[\d+\]', replace_emoji, text)
        return formatted_text.replace("\n", "<br>")

    @pyqtSlot()
    def send_message(self):
        message_text = self.message_input.toPlainText().strip()
        if not message_text: return
        self.append_message(message_text, self.own_username, is_own=True)
        self.network_core.send_message(message_text, self.target_ip)
        self.message_input.clear()

    def append_message(self, text, sender_name, is_own=False):
        sender_html = f'<p style="color: green; margin-bottom: 0;"><b>{sender_name} (我):</b></p>' if is_own else f'<p style="color: blue; margin-bottom: 0;"><b>{sender_name}:</b></p>'
        content_html = self.format_text_for_display(text)
        full_html = f'{sender_html}<div style="margin-left: 10px;">{content_html}</div>'
        self.message_display.append(full_html)

    def append_image(self, image_path, sender_name, is_own=False):
        header = f'<p style="color: green;"><b>{sender_name} (我) 发送了截图:</b></p>' if is_own else f'<p style="color: blue;"><b>{sender_name} 发送了截图:</b></p>'
        self.message_display.append(header)

        # 将图片放在第二行显示
        image_url = QUrl.fromLocalFile(os.path.abspath(image_path))
        cursor = self.message_display.textCursor()
        cursor.movePosition(QTextCursor.End)
        cursor.insertBlock()  # 新起一行（第二行）

        image_format = QTextImageFormat()
        image_format.setName(image_url.toString())
        cursor.insertImage(image_format)

        cursor.insertBlock()  # 在图片后再换一行，避免与后续文本挤在一起
        self.message_display.setTextCursor(cursor)
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
        time.sleep(0.2)
        self.screenshot_tool = ScreenshotTool()
        self.screenshot_tool.screenshot_taken.connect(self.handle_screenshot_taken)
        self.screenshot_tool.show()

    @pyqtSlot(str)
    def handle_screenshot_taken(self, image_path):
        self.show()
        self.main_window.show()
        self.activateWindow()
        self.append_image(image_path, self.own_username, is_own=True)
        self.send_file_request.emit(self.target_ip, image_path)

    @pyqtSlot(dict, str)
    def handle_file_request(self, msg, sender_ip):
        parts = msg['extra_msg'].split(':')
        filename, filesize = parts[0], parts[1]
        
        title = '文件传输请求'
        content = f"用户 {self.target_user_info['sender']} ({sender_ip}) 想传送文件:\n" \
                  f"名称: {filename}\n大小: {filesize} bytes\n\n您是否同意接收？"
        w = MessageBox(title, content, self)
        # 降低阻塞风险，允许点击遮罩关闭、可拖拽
        try:
            w.setClosableOnMaskClicked(True)
            w.setDraggable(True)
        except Exception:
            pass
        # 暫停動畫，避免模態對話框期間 GIF 刷新阻塞事件循環
        try:
            if hasattr(self, 'message_display') and hasattr(self.message_display, 'pause_animations'):
                self.message_display.pause_animations(True)
        except Exception:
            pass

        # 使用非模態方式顯示，避免阻塞事件循環
        def on_accept():
            # 恢復動畫
            try:
                if hasattr(self, 'message_display'):
                    self.message_display.setUpdatesEnabled(True)
                    if hasattr(self.message_display, 'start_animations'):
                        self.message_display.start_animations()
            except Exception:
                pass

            self.receiver_thread = FileReceiver("cache", filename, filesize)
            self.receiver_thread.ready_to_receive.connect(
                lambda port, save_path: self.send_file_ready.emit(port, msg['packet_no'], sender_ip)
            )
            self.receiver_thread.transfer_finished.connect(
                lambda path: self.append_image(path, self.target_user_info['sender'], is_own=False)
            )
            self.receiver_thread.start()

        def on_reject():
            try:
                if hasattr(self, 'message_display'):
                    self.message_display.setUpdatesEnabled(True)
                    if hasattr(self.message_display, 'start_animations'):
                        self.message_display.start_animations()
            except Exception:
                pass

        try:
            # 暫停與禁用更新
            if hasattr(self, 'message_display'):
                self.message_display.setUpdatesEnabled(False)
                if hasattr(self.message_display, 'stop_animations'):
                    self.message_display.stop_animations()
        except Exception:
            pass

        w.yesSignal.connect(on_accept)
        w.cancelSignal.connect(on_reject)
        w.show()

    @pyqtSlot(dict, str)
    def handle_file_ready(self, msg, sender_ip):
        parts = msg['extra_msg'].split(':')
        tcp_port, original_packet_no = parts[0], int(parts[1])
        filepath = self.pending_files.get(original_packet_no)
        if filepath:
            self.sender_thread = FileSender(sender_ip, tcp_port, filepath)
            self.sender_thread.start()
            self.pending_files.pop(original_packet_no)

    def handle_link_clicked(self, url):
        if url.scheme() == 'file':
            QDesktopServices.openUrl(url)

    def closeEvent(self, event):
        if self.target_ip in self.main_window.chat_windows:
            del self.main_window.chat_windows[self.target_ip]
        super().closeEvent(event)