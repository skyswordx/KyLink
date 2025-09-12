import socket
from PyQt5.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QTreeWidget, 
                             QTreeWidgetItem, QPushButton, QLineEdit, QLabel, 
                             QHBoxLayout, QMessageBox)
from PyQt5.QtCore import pyqtSlot, Qt
from core.network import NetworkCore
from ui.chat_window import ChatWindow
from ui.group_chat_dialog import GroupChatDialog

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PyFeiQ")
        self.setGeometry(200, 200, 300, 500)
        
        self.users = {} 
        self.chat_windows = {}

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        
        self.search_input = QLineEdit()
        self.search_input.setPlaceholderText("搜索用户、IP...")
        
        self.user_tree_widget = QTreeWidget()
        self.user_tree_widget.setHeaderLabels(['在线好友'])
        self.user_tree_widget.setIndentation(15)
        
        bottom_layout = QHBoxLayout()
        self.group_message_button = QPushButton("群发消息")
        bottom_layout.addWidget(self.group_message_button)
        bottom_layout.addStretch(1)
        self.status_label = QLabel("正在初始化...")
        bottom_layout.addWidget(self.status_label)
        
        layout.addWidget(self.search_input)
        layout.addWidget(self.user_tree_widget)
        layout.addLayout(bottom_layout)
        
        self.username = "PyUser"
        self.groupname = "开发组"
        self.hostname = socket.gethostname()
        
        self.network_thread = NetworkCore(self.username, self.hostname, self.groupname)
        self.setup_connections()
        self.network_thread.start()

    def setup_connections(self):
        """
        设置信号和槽的连接
        """
        self.network_thread.user_online.connect(self.handle_user_online)
        self.network_thread.user_offline.connect(self.handle_user_offline)
        self.network_thread.message_received.connect(self.handle_message_received)
        
        # --- 关键修复：添加下面这行代码 ---
        self.user_tree_widget.itemDoubleClicked.connect(self.open_chat_window_from_tree)
        # ------------------------------------

        self.search_input.textChanged.connect(self.filter_user_list)
        self.group_message_button.clicked.connect(self.open_group_chat_dialog)

    def closeEvent(self, event):
        for chat_win in self.chat_windows.values():
            chat_win.close()
        self.network_thread.stop()
        event.accept()

    def update_user_list(self):
        self.user_tree_widget.clear()
        groups = {}
        sorted_users = sorted(self.users.values(), key=lambda u: (u.get('group_name', ''), u.get('display_name', '')))
        
        for user_info in sorted_users:
            group_name = user_info.get('group_name', '我的好友')
            display_name = user_info.get('display_name', user_info.get('sender', 'Unknown'))
            ip = user_info['ip']

            if group_name not in groups:
                group_item = QTreeWidgetItem(self.user_tree_widget)
                group_item.setText(0, f"{group_name} [0]")
                group_item.setData(0, Qt.UserRole, {'type': 'group', 'name': group_name})
                groups[group_name] = group_item
            
            group_item = groups[group_name]

            user_item = QTreeWidgetItem(group_item)
            user_item.setText(0, f"{display_name} ({ip})")
            user_item.setData(0, Qt.UserRole, {'type': 'user', 'ip': ip})

        for group_name, group_item in groups.items():
            count = group_item.childCount()
            group_item.setText(0, f"{group_name} [{count}]")
        
        self.user_tree_widget.expandAll()
        self.status_label.setText(f"在线用户: {len(self.users)}")
        self.filter_user_list()

    @pyqtSlot(str)
    def filter_user_list(self, text=""):
        search_text = self.search_input.text().lower()
        root = self.user_tree_widget.invisibleRootItem()
        for i in range(root.childCount()):
            group_item = root.child(i)
            has_visible_child = False
            for j in range(group_item.childCount()):
                user_item = group_item.child(j)
                item_text = user_item.text(0).lower()
                if search_text in item_text:
                    user_item.setHidden(False)
                    has_visible_child = True
                else:
                    user_item.setHidden(True)
            group_item.setHidden(not has_visible_child)

    @pyqtSlot(dict, str)
    def handle_user_online(self, msg, ip):
        msg['ip'] = ip
        display_name = msg.get('display_name', msg.get('sender'))

        if ip not in self.users or self.users[ip].get('display_name') != display_name or self.users[ip].get('group_name') != msg.get('group_name'):
            print(f"用户上线或更新: {display_name} @ {ip} in group '{msg.get('group_name')}'")
            self.users[ip] = msg
            self.update_user_list()

    @pyqtSlot(dict, str)
    def handle_user_offline(self, msg, ip):
        if ip in self.users:
            print(f"用户下线: {self.users[ip]['display_name']} @ {ip}")
            if ip in self.chat_windows:
                self.chat_windows[ip].close()
                del self.chat_windows[ip]
            del self.users[ip]
            self.update_user_list()
            
    @pyqtSlot(dict, str)
    def handle_message_received(self, msg, ip):
        sender_name = msg.get('display_name', msg.get('sender', 'Unknown'))
        print(f"收到来自 {sender_name} 的消息: {msg['extra_msg']}")
        
        if ip not in self.chat_windows or not self.chat_windows[ip].isVisible():
            self.open_chat_window(ip)

        if ip in self.chat_windows:
            self.chat_windows[ip].append_message(msg['extra_msg'], sender_name)
            self.chat_windows[ip].activateWindow()

    def open_chat_window(self, target_ip):
        """
        打开或激活一个聊天窗口 (增加调试信息)
        """
        print(f"[调试] ==> 'open_chat_window' 函数被调用, 目标IP: {target_ip}")

        if target_ip not in self.users:
            print(f"[调试] 错误: 目标IP {target_ip} 不在当前用户列表中。无法打开聊天。")
            return

        print(f"[调试] 目标用户 {self.users[target_ip].get('display_name')} 存在。继续...")

        if target_ip in self.chat_windows and self.chat_windows[target_ip].isVisible():
            print(f"[调试] 聊天窗口已存在且可见，正在激活它。")
            self.chat_windows[target_ip].activateWindow()
        else:
            print(f"[调试] 正在为 {target_ip} 创建一个新的聊天窗口实例...")
            target_user_info = self.users[target_ip].copy()
            target_user_info['sender'] = target_user_info.get('display_name', target_user_info.get('sender', 'Unknown'))
            
            chat_win = ChatWindow(self.username, target_user_info, target_ip, self.network_thread, self)
            
            # 使用 lambda 简化连接
            chat_win.destroyed.connect(lambda: self.chat_windows.pop(target_ip, None))

            self.chat_windows[target_ip] = chat_win
            chat_win.show()
            print(f"[调试] 新的聊天窗口应该已经显示。")

    @pyqtSlot(QTreeWidgetItem, int)
    def open_chat_window_from_tree(self, item, column):
        """
        从用户列表中双击打开聊天窗口的槽函数 (增加调试信息)
        """
        print("[调试] ==> 检测到列表项双击事件！")
        item_data = item.data(0, Qt.UserRole)
        print(f"[调试] 被双击项的数据是: {item_data}")
        
        if item_data and item_data.get('type') == 'user':
            print(f"[调试] 判断为用户项，准备为IP {item_data['ip']} 打开聊天窗口...")
            self.open_chat_window(item_data['ip'])
        else:
            print("[调试] 判断为分组项或数据为空，不执行任何操作。")
            
    @pyqtSlot()
    def open_group_chat_dialog(self):
        selected_item = self.user_tree_widget.currentItem()
        if not selected_item:
            QMessageBox.warning(self, "操作提示", "请先在列表中选择一个分组或一位用户。")
            return
            
        item_data = selected_item.data(0, Qt.UserRole)
        group_item = None

        if item_data.get('type') == 'group':
            group_item = selected_item
        elif item_data.get('type') == 'user':
            group_item = selected_item.parent()

        if not group_item:
            return

        group_name = group_item.data(0, Qt.UserRole)['name']
        recipients = []
        for i in range(group_item.childCount()):
            user_item = group_item.child(i)
            user_data = user_item.data(0, Qt.UserRole)
            user_ip = user_data['ip']
            if user_ip in self.users:
                 recipients.append({
                    'ip': user_ip,
                    'name': self.users[user_ip].get('display_name')
                })

        if not recipients:
            QMessageBox.information(self, "提示", "该分组中没有可发送消息的用户。")
            return

        dialog = GroupChatDialog(self.username, group_name, recipients, self.network_thread, self)
        dialog.exec_()
