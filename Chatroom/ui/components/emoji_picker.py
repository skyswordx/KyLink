from PyQt5.QtWidgets import QDialog, QGridLayout, QLabel
from PyQt5.QtCore import pyqtSignal, Qt
from PyQt5.QtGui import QMovie

class ClickableLabel(QLabel):
    """
    一个可点击的QLabel类，用于显示表情
    """
    clicked = pyqtSignal(str) # 自定义信号，点击时发射表情代码

    def __init__(self, code, movie, parent=None):
        super().__init__(parent)
        self.code = code
        self.setMovie(movie)
        movie.start()

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.clicked.emit(self.code)
        super().mousePressEvent(event)

class EmojiPicker(QDialog):
    emoji_selected = pyqtSignal(str)

    def __init__(self, emoji_manager, parent=None):
        super().__init__(parent)
        self.emoji_manager = emoji_manager
        self.setWindowTitle("选择表情")
        self.setFixedSize(400, 300)
        
        # 这个列表至关重要，它能防止QMovie对象被垃圾回收机制提前销毁
        self.movies = [] 
        
        self.init_ui()

    def init_ui(self):
        layout = QGridLayout(self)
        self.setLayout(layout)
        
        emojis = self.emoji_manager.get_all_emojis()
        if not emojis:
            print("[调试] EmojiPicker: 表情地图为空，无法创建表情。")
            return

        print(f"[调试] EmojiPicker: 正在为 {len(emojis)} 个表情创建预览...")
        
        try:
            # 按表情代码中的数字进行排序，确保顺序正确
            sorted_codes = sorted(emojis.keys(), key=lambda x: int(x.strip('[]')))
        except ValueError:
            # 如果代码格式不正确，则按原样排序
            sorted_codes = sorted(emojis.keys())
        
        row, col = 0, 0
        for code in sorted_codes:
            path = self.emoji_manager.get_emoji_path(code)
            if path:
                movie = QMovie(path)
                self.movies.append(movie) # 保持对movie的引用

                label = ClickableLabel(code, movie)
                label.setFixedSize(32, 32)
                label.setScaledContents(True)
                
                label.clicked.connect(self.on_emoji_click)
                
                layout.addWidget(label, row, col)
                
                col += 1
                if col >= 10: # 每行10个表情
                    col = 0
                    row += 1
        print("[调试] EmojiPicker: 表情预览创建完成。")

    def on_emoji_click(self, code):
        self.emoji_selected.emit(code)
        self.accept() # 关闭对话框
