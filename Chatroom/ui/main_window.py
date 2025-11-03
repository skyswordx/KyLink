import socket
import os
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QTreeWidgetItem
from PyQt5.QtCore import pyqtSlot, Qt
from PyQt5.QtGui import QIcon
from qfluentwidgets import (FluentWindow, NavigationItemPosition, 
                            LineEdit, TreeWidget, BodyLabel, PushButton,
                            FluentIcon as FIF)

from core.network import NetworkCore
from ui.chat_window import ChatWindow
from ui.group_chat_dialog import GroupChatDialog
from ui.about_interface import AboutInterface
from utils.config import cfg, APP_NAME, AUTHOR, CONFIG_FILE

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
        self.user_tree_widget.setHeaderLabels(['在线好友'])

        bottom_layout = QHBoxLayout()
        self.group_message_button = PushButton("群发消息", self)
        self.group_message_button.setIcon(FIF.SEND)
        self.status_label = BodyLabel("正在初始化...", self)
        bottom_layout.addWidget(self.group_message_button)
        bottom_layout.addStretch(1)
        bottom_layout.addWidget(self.status_label)

        # --- 整體佈局 ---
        layout = QVBoxLayout(self)
        layout.setContentsMargins(15, 15, 15, 15)
        layout.setSpacing(10)
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
        self.setWindowTitle(APP_NAME)
        self.setWindowIcon(QIcon(":/qfluentwidgets/images/logo.png"))
        self.setGeometry(300, 300, 900, 700)
        self.setMinimumWidth(760)

        # Enable acrylic effect
        self.navigationInterface.setAcrylicEnabled(True)

        # --- 核心邏輯初始化 ---
        self.users = {} 
        self.chat_windows = {}
        self.username = cfg.get(cfg.username)
        self.groupname = cfg.get(cfg.groupname)
        # 获取主机名：优先使用配置的自定义主机名，否则使用系统主机名
        system_hostname = socket.gethostname()
        self.hostname = cfg.get(cfg.display_hostname) if cfg.get(cfg.display_hostname) else system_hostname
        self.system_hostname = system_hostname  # 保存系统主机名用于显示
        
        # 創建子介面
        self.chat_interface = ChatInterface(self)
        self.about_interface = AboutInterface(self)
        
        # 添加到堆疊視窗
        self.stackedWidget.addWidget(self.chat_interface)
        self.stackedWidget.addWidget(self.about_interface)

        # 初始化導覽列
        self.initNavigation()
        
        # 获取显示名称（如果配置了的话）
        display_name_value = cfg.get(cfg.display_name)
        actual_display_name = display_name_value if display_name_value else self.username
        
        # 設置網路連接
        self.network_thread = NetworkCore(self.username, self.hostname, self.groupname)
        # 如果配置了显示名称，更新网络线程的显示名称（用于发送消息）
        if display_name_value:
            self.network_thread.display_name = actual_display_name
        self.setup_connections()
        self.network_thread.start()

    def initNavigation(self):
        """初始化導覽列"""
        # 添加導航項目
        self.addSubInterface(
            self.chat_interface,
            FIF.MESSAGE,
            self.tr('聊天'),
            position=NavigationItemPosition.TOP
        )
        
        # 添加分隔線
        self.navigationInterface.addSeparator()
        
        # 添加關於介面到底部 - 使用不同的图标避免与用户图标重复
        self.addSubInterface(
            self.about_interface,
            FIF.HELP,  # 使用 HELP 图标，表示关于/帮助页面
            self.tr('关于'),
            position=NavigationItemPosition.BOTTOM
        )
        
        # 添加用戶頭像到底部導航欄
        # 使用用户相关的图标
        try:
            user_icon = FIF.PERSON  # 优先使用 PERSON 图标
        except AttributeError:
            try:
                user_icon = FIF.CONTACT
            except AttributeError:
                try:
                    user_icon = FIF.ACCOUNT
                except AttributeError:
                    user_icon = FIF.HOME  # 最后的后备图标
                
        self.navigationInterface.addItem(
            routeKey='user_avatar',
            icon=user_icon,
            text=self.username,
            onClick=self.show_user_info,  # 添加点击事件
            selectable=False,
            tooltip=f"{self.username}\n{self.hostname}\n点击查看并配置用户信息",
            position=NavigationItemPosition.BOTTOM
        )

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
        self.chat_interface.status_label.setText(f"在线用户: {len(self.users)}")
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
        # 优先从用户列表中获取显示名称，如果不在列表中则使用消息中的发送者名称
        if ip in self.users:
            sender_name = self.users[ip].get('display_name', self.users[ip].get('sender', 'Unknown'))
        else:
            sender_name = msg.get('sender', 'Unknown')
        
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
            # 生成文件ID
            file_id = self.network_thread.get_file_id()
            # 使用标准IPMSG协议发送文件请求，返回packet_no
            packet_no = self.network_thread.send_file_request(filepath, filesize, file_id, target_ip)
            if target_ip in self.chat_windows:
                # 存储文件信息，使用(packet_no, file_id)作为键
                # 注意：文件已注册到FileTransferManager，这里只是记录一下
                self.chat_windows[target_ip].pending_files[(packet_no, file_id)] = {
                    'path': filepath,
                    'filename': filename,
                    'filesize': filesize
                }
        except Exception as e:
            from qfluentwidgets import InfoBar, InfoBarPosition
            InfoBar.error(
                title=self.tr('错误'),
                content=self.tr(f'无法发送文件请求: {e}'),
                duration=5000,
                parent=self,
                position=InfoBarPosition.TOP
            )

    @pyqtSlot(QTreeWidgetItem, int)
    def open_chat_window_from_tree(self, item, column):
        item_data = item.data(0, Qt.UserRole)
        if item_data and item_data.get('type') == 'user':
            self.open_chat_window(item_data['ip'])
            
    @pyqtSlot()
    def open_group_chat_dialog(self):
        selected_item = self.chat_interface.user_tree_widget.currentItem()
        if not selected_item:
            from qfluentwidgets import InfoBar, InfoBarPosition
            InfoBar.warning(
                title=self.tr('操作提示'),
                content=self.tr('请先在列表中选择一个分组或一位用户。'),
                duration=3000,
                parent=self,
                position=InfoBarPosition.TOP
            )
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
            from qfluentwidgets import InfoBar, InfoBarPosition
            InfoBar.warning(
                title=self.tr('提示'),
                content=self.tr('该分组中没有可发送消息的用户。'),
                duration=3000,
                parent=self,
                position=InfoBarPosition.TOP
            )
            return

        dialog = GroupChatDialog(self.username, group_name, recipients, self.network_thread, self)
        dialog.exec_()
    
    @pyqtSlot()
    def show_user_info(self):
        """显示用户信息对话框（包含配置编辑功能）"""
        from qframelesswindow import FramelessDialog, StandardTitleBar
        from qfluentwidgets import (SubtitleLabel, BodyLabel, 
                                    PrimaryPushButton, PushButton, LineEdit, CheckBox,
                                    isDarkTheme, qconfig)
        from PyQt5.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout
        
        dialog = FramelessDialog(self)
        dialog.setTitleBar(StandardTitleBar(dialog))
        dialog.titleBar.raise_()
        dialog.setWindowTitle(self.tr('用户信息与配置'))
        dialog.setMinimumWidth(480)
        dialog.setMinimumHeight(500)
        
        # 创建内容widget
        content = QWidget()
        layout = QVBoxLayout(content)
        layout.setSpacing(15)
        layout.setContentsMargins(20, 20, 20, 20)
        
        # 用户信息部分
        info_title = SubtitleLabel(self.tr('当前用户信息'), content)
        layout.addWidget(info_title)
        
        # 用户名配置
        username_layout = QHBoxLayout()
        username_label = BodyLabel(f"{self.tr('用户名')}: ", content)
        username_input = LineEdit(content)
        username_input.setText(self.username)
        username_input.setPlaceholderText(self.tr('请输入用户名'))
        username_layout.addWidget(username_label)
        username_layout.addWidget(username_input, 1)
        layout.addLayout(username_layout)
        
        # 显示名称配置
        display_name_layout = QHBoxLayout()
        display_name_label = BodyLabel(f"{self.tr('显示名称')}: ", content)
        display_name_input = LineEdit(content)
        display_name_value = cfg.get(cfg.display_name)
        display_name_input.setText(display_name_value if display_name_value else "")
        display_name_input.setPlaceholderText(self.tr('留空则使用用户名'))
        display_name_layout.addWidget(display_name_label)
        display_name_layout.addWidget(display_name_input, 1)
        layout.addLayout(display_name_layout)
        
        # 主机名配置
        hostname_layout = QHBoxLayout()
        hostname_label = BodyLabel(f"{self.tr('主机名')}: ", content)
        hostname_input = LineEdit(content)
        hostname_input.setText(self.hostname)
        hostname_input.setPlaceholderText(self.tr('留空则使用系统主机名'))
        hostname_layout.addWidget(hostname_label)
        hostname_layout.addWidget(hostname_input, 1)
        layout.addLayout(hostname_layout)
        
        # 系统主机名显示（只读）
        system_hostname_label = BodyLabel(f"{self.tr('系统主机名')}: {self.system_hostname}", content)
        system_hostname_label.setStyleSheet("color: gray;")
        layout.addWidget(system_hostname_label)
        
        # 组名配置
        group_layout = QHBoxLayout()
        group_label = BodyLabel(f"{self.tr('组名')}: ", content)
        group_input = LineEdit(content)
        group_input.setText(self.groupname)
        group_input.setPlaceholderText(self.tr('请输入组名'))
        group_layout.addWidget(group_label)
        group_layout.addWidget(group_input, 1)
        layout.addLayout(group_layout)
        
        # 自动检测主机名选项
        auto_detect_checkbox = CheckBox(self.tr('自动检测主机名（取消勾选以使用自定义主机名）'), content)
        auto_detect_checkbox.setChecked(cfg.get(cfg.auto_detect_hostname))
        layout.addWidget(auto_detect_checkbox)
        
        layout.addSpacing(10)
        
        # 在线用户数（只读）
        online_count_label = BodyLabel(f"{self.tr('在线用户数')}: {len(self.users)}", content)
        layout.addWidget(online_count_label)
        
        layout.addStretch()
        
        # 按钮
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        cancel_button = PushButton(self.tr('取消'), content)
        save_button = PrimaryPushButton(self.tr('保存并重启'), content)
        button_layout.addWidget(cancel_button)
        button_layout.addWidget(save_button)
        layout.addLayout(button_layout)
        
        # 设置对话框布局
        dialog_layout = QVBoxLayout(dialog)
        dialog_layout.setContentsMargins(0, dialog.titleBar.height(), 0, 0)
        dialog_layout.addWidget(content)
        
        # 设置样式
        if isDarkTheme():
            dialog.setStyleSheet("""
                FramelessDialog {
                    background-color: rgb(32, 32, 32);
                }
            """)
        
        # 连接信号
        def save_and_restart():
            try:
                # 保存配置
                new_username = username_input.text().strip() or "User"
                new_display_name = display_name_input.text().strip()
                new_hostname = hostname_input.text().strip()
                new_groupname = group_input.text().strip() or "局域网"
                auto_detect = auto_detect_checkbox.isChecked()
                
                cfg.set(cfg.username, new_username)
                cfg.set(cfg.display_name, new_display_name)
                cfg.set(cfg.display_hostname, new_hostname if not auto_detect else "")
                cfg.set(cfg.groupname, new_groupname)
                cfg.set(cfg.auto_detect_hostname, auto_detect)
                
                # 保存配置文件
                # 使用 qconfig.save() 保存配置
                try:
                    qconfig.save(cfg)
                except Exception as save_error:
                    # 如果 qconfig.save() 失败，尝试手动保存
                    import json
                    config_data = {}
                    config_data["User"] = {
                        "Username": new_username,
                        "DisplayName": new_display_name,
                        "DisplayHostname": new_hostname if not auto_detect else "",
                        "GroupName": new_groupname,
                        "AutoDetectHostname": auto_detect
                    }
                    # 保留其他配置项
                    try:
                        if os.path.exists(CONFIG_FILE):
                            with open(CONFIG_FILE, 'r', encoding='utf-8') as f:
                                existing_config = json.load(f)
                                existing_config.update(config_data)
                                config_data = existing_config
                    except:
                        pass
                    
                    with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
                        json.dump(config_data, f, ensure_ascii=False, indent=2)
                
                # 先关闭对话框
                dialog.accept()
                
                # 使用非阻塞方式显示提示消息
                from qfluentwidgets import InfoBar, InfoBarPosition
                InfoBar.success(
                    title=self.tr('配置已保存'),
                    content=self.tr('部分配置需要重启应用程序才能生效。'),
                    duration=3000,  # 3秒后自动消失
                    parent=self,
                    position=InfoBarPosition.TOP
                )
                
            except Exception as e:
                # 如果保存失败，显示错误信息
                dialog.accept()
                from qfluentwidgets import InfoBar, InfoBarPosition
                InfoBar.error(
                    title=self.tr('保存失败'),
                    content=self.tr(f'保存配置时发生错误：{str(e)}'),
                    duration=5000,  # 5秒后自动消失
                    parent=self,
                    position=InfoBarPosition.TOP
                )
        
        cancel_button.clicked.connect(dialog.reject)
        save_button.clicked.connect(save_and_restart)
        
        dialog.exec_()