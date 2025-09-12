import os
from PyQt5.QtWidgets import QDialog, QGridLayout, QPushButton, QToolButton
from PyQt5.QtGui import QIcon
from PyQt5.QtCore import QSize, pyqtSignal

class EmojiPicker(QDialog):
    """
    一个用于选择表情的弹出式对话框
    """
    emoji_selected = pyqtSignal(str) # 定义一个信号，当表情被选中时发射

    def __init__(self, emoji_manager, parent=None):
        super().__init__(parent)
        self.emoji_manager = emoji_manager
        self.setWindowTitle("选择表情")
        self.setFixedSize(400, 300)
        
        self.init_ui()

    def init_ui(self):
        layout = QGridLayout(self)
        layout.setSpacing(5)
        
        all_emojis = self.emoji_manager.get_all_emojis()
        
        # 定义网格布局的行列数
        cols = 10
        row, col = 0, 0
        
        # 按表情代码排序，确保顺序一致
        for code in sorted(all_emojis.keys()):
            path = all_emojis[code]
            if path and os.path.exists(path):
                button = QToolButton()
                button.setIcon(QIcon(path))
                button.setIconSize(QSize(32, 32))
                button.setToolTip(f"表情代码: {code}")
                # 使用lambda传递参数
                button.clicked.connect(lambda _, c=code: self.on_emoji_click(c))
                
                layout.addWidget(button, row, col)
                
                col += 1
                if col >= cols:
                    col = 0
                    row += 1
                    
    def on_emoji_click(self, code):
        """
        当一个表情按钮被点击时调用
        """
        self.emoji_selected.emit(code)
        self.accept() # 关闭对话框
