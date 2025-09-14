import socket
import os
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QTreeWidgetItem
from PyQt5.QtCore import pyqtSlot, Qt
from PyQt5.QtGui import QIcon
from qfluentwidgets import (FluentWindow, NavigationItemPosition, 
                            MessageBox, LineEdit, TreeWidget, BodyLabel, PushButton,
                            FluentIcon as FIF)

from core.network import NetworkCore
from ui.chat_window import ChatWindow
from ui.group_chat_dialog import GroupChatDialog

class ChatInterface(QWidget):
    """
    一個獨立的 Widget，用於容納原主視窗的聊天相關元件 (用戶列表等)。
    """
    def __init__(self, main_window_ref):
        super().__init__()
        self.main_window = main_window_ref
        
        self.setObjectName("ChatInterface")
        
        # --- UI 元件 ---
        self.search_input = LineEdit(self)
        self.search_input.setPlaceholderText("搜索用戶、IP...")
        self.search_input.setClearButtonEnabled(True)

        self.user_tree_widget = TreeWidget(self)
        self.user_tree_widget.setHeaderLabels(['在線好友'])

        bottom_layout = QHBoxLayout()
        self.group_message_button = PushButton("群發消息", self)
        self.status_label = BodyLabel("正在初始化...", self)
        bottom_layout.addWidget(self.group_message_button)
        bottom_layout.addStretch(1)
        bottom_layout.addWidget(self.status_label)

        # --- 整體佈局 ---
        layout = QVBoxLayout(self)
        layout.addWidget(self.search_input)
        layout.addWidget(self.user_tree_widget)
        layout.addLayout(bottom_layout)
        
        # --- 信號連接 ---
        self.group_message_button.clicked.connect(self.main_window.open_group_chat_dialog)
        self.search_input.textChanged.connect(self.main_window.filter_user_list)
        self.user_tree_widget.itemDoubleClicked.connect(self.main_window.open_chat_window_from_tree)


class MainWindow(FluentWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("PyFeiQ Fluent")
        self.setWindowIcon(QIcon(":/qfluentwidgets/images/logo.png"))
        self.setGeometry(300, 300, 450, 700)

        # 創建聊天主介面並將其添加到堆疊視窗中
        self.chat_interface = ChatInterface(self)
        self.stackedWidget.addWidget(self.chat_interface)

        # 設置導覽列
        self.navigationInterface.addItem(
            routeKey='chat_interface',
            icon=FIF.MESSAGE,
            text='聊天',
            onClick=lambda: self.stackedWidget.setCurrentWidget(self.chat_interface),
            position=NavigationItemPosition.TOP
        )
        
        # --- 核心邏輯初始化 ---
        self.users = {} 
        self.chat_windows = {}
        self.username = "PyUser"
        self.groupname = "開發組"
        self.hostname = socket.gethostname()
        
        self.network_thread = NetworkCore(self.username, self.hostname, self.groupname)
        self.setup_connections()
        self.network_thread.start()

    def setup_connections(self):
        self.network_thread.user_online.connect(self.handle_user_online)
        self.network_thread.user_offline.connect(self.handle_user_offline)
        self.network_thread.message_received.connect(self.handle_message_received)
        self.network_thread.file_request_received.connect(self.handle_file_request_route)
        self.network_thread.file_receiver_ready.connect(self.handle_file_ready_route)

    def closeEvent(self, event):
        for chat_win in self.chat_windows.values():
            chat_win.close()
        self.network_thread.stop()
        event.accept()

    @pyqtSlot()
    def update_user_list(self):
        self.chat_interface.user_tree_widget.clear()
        groups = {}
        # 確保在排序前 self.users 的值是穩定的
        users_list = list(self.users.values())
        sorted_users = sorted(users_list, key=lambda u: (u.get('group_name', ''), u.get('display_name', '')))
        
        for user_info in sorted_users:
            group_name = user_info.get('group_name', '我的好友')
            display_name = user_info.get('display_name', user_info.get('sender', 'Unknown'))
            ip = user_info['ip']

            if group_name not in groups:
                # 使用正確的 QTreeWidgetItem
                group_item = QTreeWidgetItem(self.chat_interface.user_tree_widget)
                group_item.setText(0, f"{group_name} [0]")
                group_item.setData(0, Qt.UserRole, {'type': 'group', 'name': group_name})
                groups[group_name] = group_item
            
            group_item = groups[group_name]

            # 使用正確的 QTreeWidgetItem
            user_item = QTreeWidgetItem(group_item)
            user_item.setText(0, f"{display_name} ({ip})")
            user_item.setData(0, Qt.UserRole, {'type': 'user', 'ip': ip})

        for group_name, group_item in groups.items():
            count = group_item.childCount()
            group_item.setText(0, f"{group_name} [{count}]")
        
        self.chat_interface.user_tree_widget.expandAll()
        self.chat_interface.status_label.setText(f"在線用戶: {len(self.users)}")
        self.filter_user_list()

    @pyqtSlot(str)
    def filter_user_list(self, text=""):
        search_text = self.chat_interface.search_input.text().lower()
        root = self.chat_interface.user_tree_widget.invisibleRootItem()
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
            self.users[ip] = msg
            self.update_user_list()

    @pyqtSlot(dict, str)
    def handle_user_offline(self, msg, ip):
        if ip in self.users:
            if ip in self.chat_windows:
                self.chat_windows[ip].close()
            del self.users[ip]
            self.update_user_list()
            
    @pyqtSlot(dict, str)
    def handle_message_received(self, msg, ip):
        sender_name = msg.get('display_name', msg.get('sender', 'Unknown'))
        if ip not in self.chat_windows or not self.chat_windows[ip].isVisible():
            self.open_chat_window(ip)
        if ip in self.chat_windows:
            self.chat_windows[ip].append_message(msg['extra_msg'], sender_name)
            self.chat_windows[ip].activateWindow()

    @pyqtSlot(dict, str)
    def handle_file_request_route(self, msg, sender_ip):
        if sender_ip not in self.chat_windows or not self.chat_windows[sender_ip].isVisible():
            self.open_chat_window(sender_ip)
        if sender_ip in self.chat_windows:
            self.chat_windows[sender_ip].activateWindow()
            self.chat_windows[sender_ip].handle_file_request(msg, sender_ip)

    @pyqtSlot(dict, str)
    def handle_file_ready_route(self, msg, sender_ip):
        if sender_ip in self.chat_windows:
            self.chat_windows[sender_ip].handle_file_ready(msg, sender_ip)

    def open_chat_window(self, target_ip):
        if target_ip not in self.users: return
        if target_ip in self.chat_windows and self.chat_windows[target_ip].isVisible():
            self.chat_windows[target_ip].activateWindow()
        else:
            target_user_info = self.users[target_ip].copy()
            target_user_info['sender'] = target_user_info.get('display_name', target_user_info.get('sender', 'Unknown'))
            chat_win = ChatWindow(self.username, target_user_info, target_ip, self.network_thread, self)
            chat_win.send_file_request.connect(self.on_send_file_request)
            chat_win.send_file_ready.connect(
                lambda port, p_no, ip: self.network_thread.send_file_ready_signal(port, p_no, ip)
            )
            chat_win.destroyed.connect(lambda: self.chat_windows.pop(target_ip, None))
            self.chat_windows[target_ip] = chat_win
            chat_win.show()

    @pyqtSlot(str, str)
    def on_send_file_request(self, target_ip, filepath):
        try:
            filesize = os.path.getsize(filepath)
            filename = os.path.basename(filepath)
            self.network_thread.send_file_request(filename, filesize, target_ip)
            if target_ip in self.chat_windows:
                packet_no = self.network_thread.get_packet_no()
                self.chat_windows[target_ip].pending_files[packet_no] = filepath
        except Exception as e:
            MessageBox("錯誤", f"無法發送文件請求: {e}", self).exec()

    @pyqtSlot(QTreeWidgetItem, int)
    def open_chat_window_from_tree(self, item, column):
        item_data = item.data(0, Qt.UserRole)
        if item_data and item_data.get('type') == 'user':
            self.open_chat_window(item_data['ip'])
            
    @pyqtSlot()
    def open_group_chat_dialog(self):
        selected_item = self.chat_interface.user_tree_widget.currentItem()
        if not selected_item:
            MessageBox("操作提示", "請先在列表中選擇一個分組或一位用戶。", self).exec()
            return
            
        item_data = selected_item.data(0, Qt.UserRole)
        group_item = selected_item if item_data.get('type') == 'group' else selected_item.parent()

        if not group_item: return

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
            MessageBox("提示", "該分組中沒有可發送消息的用戶。", self).exec()
            return

        dialog = GroupChatDialog(self.username, group_name, recipients, self.network_thread, self)
        dialog.exec_()