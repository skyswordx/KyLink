import os
import json
from PyQt5.QtGui import QMovie, QTextDocument
from PyQt5.QtCore import QUrl, QObject, QByteArray, pyqtSlot

class EmojiManager(QObject):
    def __init__(self, emoji_map_path='resources/emojis/emoji_map.json', gifs_path='resources/emojis/gifs/', parent=None):
        super().__init__(parent)
        self.emoji_map = {}
        self.gifs_path = gifs_path
        self.emoji_cache = {}
        self.target_widget = None # 用于存储需要更新的控件
        self.load_emoji_map(emoji_map_path)

    def set_target_widget(self, widget):
        """
        设置一个目标控件，当GIF动画帧改变时，该控件将被更新
        """
        self.target_widget = widget

    @pyqtSlot()
    def on_frame_changed(self):
        """
        当任何一个QMovie的帧改变时，这个槽会被调用
        """
        if self.target_widget:
            # 强制目标控件进行重绘
            self.target_widget.update()

    def load_emoji_map(self, path):
        try:
            base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            full_path = os.path.join(base_dir, path)
            
            print(f"[调试] 正在尝试从以下路径加载表情地图: {full_path}")
            if not os.path.exists(full_path):
                print(f"[调试] 错误: 路径不存在!")
                return

            with open(full_path, 'r', encoding='utf-8') as f:
                self.emoji_map = json.load(f)
                print(f"[调试] 表情地图加载成功，共 {len(self.emoji_map)} 个表情。")
        except Exception as e:
            print(f"错误：无法加载表情地图 '{path}'. 原因: {e}")
            self.emoji_map = {}

    def get_emoji_path(self, code):
        filename = self.emoji_map.get(code)
        if filename:
            base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            return os.path.join(base_dir, self.gifs_path, filename)
        return None

    def get_all_emojis(self):
        return self.emoji_map

    def render_emojis(self, html_content, document: QTextDocument):
        document.addResource(QTextDocument.ImageResource, QUrl("emoji://"), self.get_emoji_resource)

    def get_emoji_resource(self, type, name: QUrl):
        code = name.toString().replace("emoji://", "")
        
        if code in self.emoji_cache:
            return self.emoji_cache[code]

        path = self.get_emoji_path(code)
        if path and os.path.exists(path):
            movie = QMovie(path, QByteArray(), self)
            
            # --- 关键修复：连接frameChanged信号到我们的更新槽 ---
            movie.frameChanged.connect(self.on_frame_changed)
            
            self.emoji_cache[code] = movie
            movie.start()
            return movie
        
        return None

