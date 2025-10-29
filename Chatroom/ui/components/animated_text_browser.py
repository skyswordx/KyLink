import os
from PyQt5.QtWidgets import QTextBrowser
from PyQt5.QtGui import QMovie, QTextDocument
from PyQt5.QtCore import QUrl, QVariant, QByteArray
from utils.emoji_manager import EmojiManager

class AnimatedTextBrowser(QTextBrowser):
    """
    一個能夠自動加載並播放GIF表情的QTextBrowser子類。
    """
    def __init__(self, parent=None):
        super().__init__(parent)
        self.emoji_manager = EmojiManager()
        self.movie_cache = {}
        print("[AnimatedTextBrowser] 初始化完成。")

    def loadResource(self, type, name: QUrl):
        """
        重寫此方法來處理自定義資源加載。
        """
        if name.scheme() == 'emoji':
            url_str = name.toString()
            if url_str in self.movie_cache:
                return self.movie_cache[url_str]
            
            movie = self.create_movie_for_emoji(name)
            if movie:
                self.movie_cache[url_str] = movie
                return movie

        return super().loadResource(type, name)

    def create_movie_for_emoji(self, url: QUrl):
        """
        為給定的 URL 創建一個 QMovie 對象。
        """
        code = url.toString().replace("emoji:", "")
        print(f"[create_movie_for_emoji] 正在為代碼 '{code}' 創建 QMovie...")

        path = self.emoji_manager.get_emoji_path(code)
        print(f"[create_movie_for_emoji] 正在為 '{code}' 尋找 GIF 路徑... 結果: {path}")

        if path and os.path.exists(path):
            movie = QMovie(path, QByteArray(), self)
            
            # --- 最終修正 ---
            # 將 frameChanged 信號連接到 on_frame_changed 方法
            # 使用 lambda 來捕獲當前的 url 變數，確保傳遞正確的 URL
            movie.frameChanged.connect(lambda: self.on_frame_changed(url))
            
            if movie.isValid():
                print(f"[create_movie_for_emoji] 為 '{code}' 創建 QMovie 成功，路徑: {path}")
                movie.start()
                return movie
            else:
                print(f"[create_movie_for_emoji] 錯誤：為 '{code}' 創建的 QMovie 無效，路徑: {path}")
        else:
            print(f"[create_movie_for_emoji] 錯誤：找不到表情 '{code}' 的 GIF 檔案，路徑: {path}")
        
        return None

    def on_frame_changed(self, url: QUrl):
        """
        當動畫幀改變時，此槽被調用，強制文檔重繪。
        """
        movie = self.movie_cache.get(url.toString())
        if movie:
            print(f"[on_frame_changed] 偵測到 '{url.toString()}' 的幀變化，正在刷新...")
            # 更新文檔的快取資源為電影的當前幀
            self.document().addResource(QTextDocument.ImageResource, url, movie.currentPixmap())
            # 觸發包含該資源的視圖部分的重繪（避免 setDocument 引起頻繁重建導致 UI 阻塞）
            self.viewport().update()

    def pause_animations(self, paused: bool):
        """暫停/恢復所有表情動畫，避免在顯示模態對話框時造成事件循環壓力。"""
        for m in self.movie_cache.values():
            try:
                m.setPaused(paused)
            except Exception:
                pass

    def stop_animations(self):
        """停止所有動畫。"""
        for m in self.movie_cache.values():
            try:
                m.stop()
            except Exception:
                pass

    def start_animations(self):
        """重新啟動所有動畫。"""
        for m in self.movie_cache.values():
            try:
                m.start()
            except Exception:
                pass