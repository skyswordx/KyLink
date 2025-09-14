from PyQt5.QtWidgets import QVBoxLayout, QHBoxLayout, QWidget
from qframelesswindow import FramelessDialog, StandardTitleBar
from qfluentwidgets import TextEdit, PushButton, SubtitleLabel, MessageBox, isDarkTheme

class GroupChatDialog(FramelessDialog):
    """
    一個用於向群組發送消息的、具有 Fluent 風格的對話方塊。
    """
    def __init__(self, own_username, group_name, recipients, network_core, main_window, parent=None):
        super().__init__(parent=parent)
        
        self.own_username = own_username
        self.group_name = group_name
        self.recipients = recipients
        self.network_core = network_core
        self.main_window = main_window

        # 設定自訂標題列和視窗標題
        self.setTitleBar(StandardTitleBar(self))
        self.titleBar.raise_()
        self.setWindowTitle(f"向組 '{self.group_name}' 發送消息")
        self.setMinimumWidth(450)
        
        self.init_ui()
        self.set_window_style()
        
    def init_ui(self):
        # FramelessDialog 需要一個佈局來管理其內容
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, self.titleBar.height(), 0, 0)
        
        # 內容容器
        content_widget = QWidget()
        layout.addWidget(content_widget)
        content_layout = QVBoxLayout(content_widget)
        content_layout.setContentsMargins(15, 15, 15, 15)

        # 接收者標籤
        recipient_names = ", ".join([r['name'] for r in self.recipients])
        info_label_text = f"<b>接收者 ({len(self.recipients)}):</b> {recipient_names}"
        info_label = SubtitleLabel(info_label_text, self)
        info_label.setWordWrap(True)

        # 訊息輸入框
        self.message_input = TextEdit(self)
        self.message_input.setPlaceholderText("在此輸入要發送的群組消息...")
        
        # 按鈕佈局
        button_layout = QHBoxLayout()
        send_button = PushButton("發送", self)
        cancel_button = PushButton("取消", self)
        button_layout.addStretch(1)
        button_layout.addWidget(cancel_button)
        button_layout.addWidget(send_button)
        
        content_layout.addWidget(info_label)
        content_layout.addWidget(self.message_input)
        content_layout.addLayout(button_layout)
        
        # --- 連接信號 ---
        send_button.clicked.connect(self.send_group_message)
        cancel_button.clicked.connect(self.reject)

    def set_window_style(self):
        """ 根據當前主題為視窗和其子元件設定樣式 """
        if isDarkTheme():
            bg_color = "rgb(43, 43, 43)"
            text_color = "white"
        else:
            bg_color = "rgb(249, 249, 249)"
            text_color = "black"

        style_sheet = f"""
            QWidget {{
                background-color: {bg_color};
                color: {text_color};
            }}
        """
        self.setStyleSheet(style_sheet)

    def send_group_message(self):
        message_text = self.message_input.toPlainText().strip()
        if not message_text:
            # 使用 Fluent 風格的 MessageBox
            MessageBox("警告", "不能發送空消息！", self).exec()
            return
            
        full_message = f"(來自群組 '{self.group_name}' 的消息)\n{message_text}"

        for recipient in self.recipients:
            target_ip = recipient['ip']
            self.network_core.send_message(full_message, target_ip)
            
            # 在本地對應的聊天視窗也顯示這條我發送的消息
            if target_ip not in self.main_window.chat_windows:
                self.main_window.open_chat_window(target_ip)
            
            if target_ip in self.main_window.chat_windows:
                chat_win = self.main_window.chat_windows[target_ip]
                chat_win.append_message(full_message, self.own_username, is_own=True)
                
        # 使用 Fluent 風格的 MessageBox
        MessageBox("成功", f"消息已發送給組 '{self.group_name}' 的 {len(self.recipients)} 位成員。", self).exec()
        self.accept()