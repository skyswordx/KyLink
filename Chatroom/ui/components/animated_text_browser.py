import os
from PyQt5.QtWidgets import QTextBrowser
from PyQt5.QtGui import QMovie
from PyQt5.QtCore import QUrl, QVariant, QByteArray
from utils.emoji_manager import EmojiManager

class AnimatedTextBrowser(QTextBrowser):
    """
    一个能够自动加载并播放GIF表情的QTextBrowser子类。
    """
    def __init__(self, parent=None):
        super().__init__(parent)
        self.emoji_manager = EmojiManager()
        self.movie_cache = {}

    def loadResource(self, type, name: QUrl):
        """
        重写此方法来处理自定义资源加载。
        当QTextBrowser在HTML中遇到<img>标签时，此方法会被调用。
        """
        # 我们只处理我们自定义的 "emoji://" 协议
        if name.scheme() == 'emoji':
            code = name.toString().replace("emoji://", "")
            return self.get_movie_for_emoji(code)

        # 对于所有其他资源，使用默认的加载行为
        return super().loadResource(type, name)

    def get_movie_for_emoji(self, code):
        """
        为给定的表情代码获取或创建一个QMovie对象。
        """
        # 如果已经为这个表情创建了动画，则直接从缓存返回
        if code in self.movie_cache:
            return self.movie_cache[code]

        # 从EmojiManager获取表情的本地文件路径
        path = self.emoji_manager.get_emoji_path(code)

        if path and os.path.exists(path):
            # 创建一个新的QMovie对象
            movie = QMovie(path, QByteArray(), self)
            # 将动画的frameChanged信号连接到我们的更新槽
            movie.frameChanged.connect(lambda: self.on_frame_changed(code))
            
            # 将动画存入缓存
            self.movie_cache[code] = movie
            movie.start()
            return QVariant(movie)

        # 如果找不到表情，返回一个空对象
        return QVariant()

    def on_frame_changed(self, code):
        """
        当动画帧改变时，此槽被调用。
        它会强制文档重新加载这个特定的图片资源。
        """
        iterator = self.document().rootFrame().begin()
        while not iterator.atEnd():
            fragment = iterator.fragment()
            if fragment.isValid():
                if fragment.charFormat().isImageFormat():
                    img_format = fragment.charFormat().toImageFormat()
                    # 如果这个图片就是我们正在更新的表情，则重新加载它
                    if img_format.name() == f"emoji://{code}":
                        # setControl()方法似乎不存在于所有版本，更可靠的方法是重新设置URL
                        cursor = self.textCursor()
                        cursor.setPosition(fragment.position())
                        cursor.setPosition(fragment.position() + fragment.length(), cursor.KeepAnchor)
                        cursor.setCharFormat(img_format)

            iterator += 1
