import os
from collections import OrderedDict
from PyQt5.QtWidgets import QTextBrowser
from PyQt5.QtGui import QMovie, QTextDocument
from PyQt5.QtCore import QUrl, QByteArray
from utils.emoji_manager import EmojiManager

class AnimatedTextBrowser(QTextBrowser):
    """
    一個能夠自動加載並播放GIF表情的QTextBrowser子類。
    優化版本：限制緩存大小，減少內存占用。
    """
    # 最大緩存表情數量（限制內存占用）
    MAX_CACHE_SIZE = 20
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.emoji_manager = EmojiManager()
        # 使用 OrderedDict 實現 LRU 緩存
        self.movie_cache = OrderedDict()

    def loadResource(self, type, name: QUrl):
        """
        重寫此方法來處理自定義資源加載。
        使用 LRU 緩存策略，限制內存占用。
        """
        if name.scheme() == 'emoji':
            url_str = name.toString()
            
            # 檢查緩存
            if url_str in self.movie_cache:
                # 移動到末尾（最近使用）
                self.movie_cache.move_to_end(url_str)
                return self.movie_cache[url_str]
            
            # 緩存未命中，創建新的 movie
            movie = self.create_movie_for_emoji(name)
            if movie:
                # 如果緩存已滿，移除最舊的項目
                if len(self.movie_cache) >= self.MAX_CACHE_SIZE:
                    oldest_url, oldest_movie = self.movie_cache.popitem(last=False)
                    oldest_movie.stop()
                    oldest_movie.deleteLater()
                
                self.movie_cache[url_str] = movie
                return movie

        return super().loadResource(type, name)

    def create_movie_for_emoji(self, url: QUrl):
        """
        為給定的 URL 創建一個 QMovie 對象（延遲加載）。
        """
        code = url.toString().replace("emoji:", "")
        path = self.emoji_manager.get_emoji_path(code)

        if path and os.path.exists(path):
            movie = QMovie(path, QByteArray(), self)
            movie.frameChanged.connect(lambda: self.on_frame_changed(url))
            
            if movie.isValid():
                movie.start()
                return movie
        
        return None

    def on_frame_changed(self, url: QUrl):
        """
        當動畫幀改變時，此槽被調用，強制文檔重繪。
        """
        movie = self.movie_cache.get(url.toString())
        if movie:
            # 更新文檔的快取資源為電影的當前幀
            self.document().addResource(QTextDocument.ImageResource, url, movie.currentPixmap())
            # 觸發包含該資源的視圖部分的重繪
            self.viewport().update()

    def pause_animations(self, paused: bool):
        """暫停/恢復所有表情動畫，避免在顯示模態對話框時造成事件循環壓力。"""
        for m in self.movie_cache.values():
            try:
                m.setPaused(paused)
            except Exception:
                pass

    def stop_animations(self):
        """停止所有動畫並清理緩存。"""
        for m in self.movie_cache.values():
            try:
                m.stop()
                m.deleteLater()
            except Exception:
                pass
        self.movie_cache.clear()

    def start_animations(self):
        """重新啟動所有動畫。"""
        for m in self.movie_cache.values():
            try:
                m.start()
            except Exception:
                pass
    
    def clear_cache(self):
        """手動清理緩存，釋放內存。"""
        self.stop_animations()
    
    def cleanup_old_cache(self):
        """清理未使用的緩存項（可選，用於定期清理）。"""
        if len(self.movie_cache) > self.MAX_CACHE_SIZE:
            # 移除最舊的一半
            items_to_remove = len(self.movie_cache) - self.MAX_CACHE_SIZE // 2
            for _ in range(items_to_remove):
                if self.movie_cache:
                    oldest_url, oldest_movie = self.movie_cache.popitem(last=False)
                    oldest_movie.stop()
                    oldest_movie.deleteLater()