import os
import json
from PyQt5.QtGui import QMovie, QTextDocument
from PyQt5.QtCore import QUrl, QSize

# 定义表情资源的基目录
BASE_EMOJI_PATH = os.path.join("resources", "emojis")
EMOJI_MAP_FILE = os.path.join(BASE_EMOJI_PATH, "emoji_map.json")

class EmojiManager:
    """
    管理表情的加载、映射和渲染
    """
    def __init__(self):
        self.emoji_map = {}
        self.url_cache = {}
        self.movie_cache = {}
        self.load_emoji_map()

    def load_emoji_map(self):
        """
        从json文件加载表情代码和文件名的映射
        """
        try:
            if os.path.exists(EMOJI_MAP_FILE):
                with open(EMOJI_MAP_FILE, 'r', encoding='utf-8') as f:
                    self.emoji_map = json.load(f)
        except Exception as e:
            print(f"加载表情映射文件失败: {e}")

    def get_emoji_path(self, code):
        """
        根据表情代码获取其文件路径
        """
        filename = self.emoji_map.get(code)
        if filename:
            return os.path.join(BASE_EMOJI_PATH, "gifs", filename)
        return None

    def get_all_emojis(self):
        """
        返回所有表情的代码和路径
        """
        return {code: self.get_emoji_path(code) for code in self.emoji_map}

    def render_emojis(self, html_content, document: QTextDocument):
        """
        解析HTML内容，加载GIF动画并显示
        :param html_content: 包含<img>标签的HTML
        :param document: QTextBrowser的document对象
        """
        # 正则查找所有img标签，但更简单的方式是使用QTextDocument的资源机制
        # 当 QTextBrowser/Edit 遇到它不认识的URL方案时，会查询 document 的 resource
        for code, path in self.get_all_emojis().items():
            if path and os.path.exists(path):
                url = QUrl(f"emoji://{code}")
                if url not in self.url_cache:
                    # 使用QMovie来处理GIF动画
                    movie = self.movie_cache.get(path)
                    if not movie:
                        movie = QMovie(path)
                        movie.setCacheMode(QMovie.CacheAll)
                        movie.setScaledSize(QSize(24, 24)) # 控制表情显示大小
                        self.movie_cache[path] = movie
                    
                    document.addResource(QTextDocument.ImageResource, url, movie)
                    # 关联动画更新到文档重绘
                    movie.frameChanged.connect(lambda: document.markContentsDirty())
                    if movie.state() != QMovie.Running:
                        movie.start()
                    self.url_cache[url] = movie
