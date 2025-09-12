from PyQt5.QtWidgets import QDialog, QVBoxLayout, QHBoxLayout, QTextEdit, QPushButton, QLabel, QMessageBox
from PyQt5.QtCore import pyqtSlot

class GroupChatDialog(QDialog):
    """
    一个简单的对话框，用于向一个组的多个用户发送消息。
    """
    def __init__(self, own_username, group_name, recipients, network_core, main_window, parent=None):
        """
        初始化
        :param own_username: 发送者自己的用户名
        :param group_name: 目标组名
        :param recipients: 接收者列表，格式为 [{'ip': '...', 'name': '...'}, ...]
        :param network_core: 网络核心实例，用于发送消息
        :param main_window: 主窗口实例，用于在对方的聊天窗口显示消息
        """
        super().__init__(parent)
        
        self.own_username = own_username
        self.group_name = group_name
        self.recipients = recipients
        self.network_core = network_core
        self.main_window = main_window # 保存对主窗口的引用

        self.setWindowTitle(f"向组 '{self.group_name}' 发送消息")
        self.setMinimumWidth(400)
        
        self.init_ui()
        
    def init_ui(self):
        layout = QVBoxLayout(self)
        
        recipient_names = ", ".join([r['name'] for r in self.recipients])
        info_label = QLabel(f"<b>接收者 ({len(self.recipients)}):</b> {recipient_names}")
        info_label.setWordWrap(True)

        self.message_input = QTextEdit()
        self.message_input.setPlaceholderText("在此输入要发送的群组消息...")
        
        button_layout = QHBoxLayout()
        send_button = QPushButton("发送")
        cancel_button = QPushButton("取消")
        button_layout.addStretch(1)
        button_layout.addWidget(cancel_button)
        button_layout.addWidget(send_button)
        
        layout.addWidget(info_label)
        layout.addWidget(self.message_input)
        layout.addLayout(button_layout)
        
        # --- 连接信号 ---
        send_button.clicked.connect(self.send_group_message)
        cancel_button.clicked.connect(self.reject)

    @pyqtSlot()
    def send_group_message(self):
        message_text = self.message_input.toPlainText().strip()
        if not message_text:
            QMessageBox.warning(self, "警告", "不能发送空消息！")
            return
            
        full_message = f"(来自群组 '{self.group_name}' 的消息)\n{message_text}"

        for recipient in self.recipients:
            target_ip = recipient['ip']
            # 1. 通过网络核心发送消息
            self.network_core.send_message(full_message, target_ip)
            
            # 2. 在本地对应的聊天窗口也显示这条我发送的消息
            #    确保聊天窗口已打开
            if target_ip not in self.main_window.chat_windows:
                self.main_window.open_chat_window(target_ip)
            
            #    追加消息
            if target_ip in self.main_window.chat_windows:
                chat_win = self.main_window.chat_windows[target_ip]
                chat_win.append_message(full_message, self.own_username, is_own=True)
                
        QMessageBox.information(self, "成功", f"消息已发送给组 '{self.group_name}' 的 {len(self.recipients)} 位成员。")
        self.accept()
