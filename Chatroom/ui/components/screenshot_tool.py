import sys
import os
import time
from PyQt5.QtWidgets import QWidget, QApplication
from PyQt5.QtGui import QPainter, QPen, QBrush, QPixmap, QGuiApplication, QColor
from PyQt5.QtCore import Qt, QRect, pyqtSignal

try:
    from PIL import ImageGrab
except ImportError:
    ImageGrab = None
    print("警告：Pillow库未安装，截图功能可能受影响。请运行 'pip install Pillow'")

class ScreenshotTool(QWidget):
    """
    一个全屏的截图工具窗口
    """
    screenshot_taken = pyqtSignal(str) # 截图完成后发射信号，参数为图片路径

    def __init__(self):
        super().__init__()
        if not ImageGrab: 
            self.close()
            return

        self.screen = QGuiApplication.primaryScreen()
        self.setGeometry(self.screen.geometry())
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        self.setCursor(Qt.CrossCursor)
        self.begin = None
        self.end = None
        self.background = self.screen.grabWindow(QApplication.desktop().winId())

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.drawPixmap(self.rect(), self.background)
        painter.setBrush(QBrush(QColor(0, 0, 0, 128)))
        painter.drawRect(self.rect())
        
        if self.begin and self.end:
            selection_rect = QRect(self.begin, self.end).normalized()
            selected_pixmap = self.background.copy(selection_rect)
            painter.drawPixmap(selection_rect.topLeft(), selected_pixmap)
            painter.setPen(QPen(Qt.red, 2, Qt.SolidLine))
            painter.setBrush(Qt.NoBrush)
            painter.drawRect(selection_rect)

    def mousePressEvent(self, event):
        self.begin = event.pos()
        self.end = self.begin
        self.update()

    def mouseMoveEvent(self, event):
        self.end = event.pos()
        self.update()

    def mouseReleaseEvent(self, event):
        self.end = event.pos()
        self.close()
        self.capture_screenshot()

    def capture_screenshot(self):
        if not self.begin or not self.end:
            return

        selection_rect = QRect(self.begin, self.end).normalized()
        final_screenshot = self.background.copy(selection_rect)
        
        # --- 修改：使用时间戳生成唯一文件名并保存到cache目录 ---
        timestamp = int(time.time() * 1000)
        filename = f"screenshot_{timestamp}.png"
        filepath = os.path.join("cache", filename)
        
        final_screenshot.save(filepath)
        print(f"截图已保存到: {filepath}")
        
        self.screenshot_taken.emit(filepath)

if __name__ == '__main__':
    app = QApplication(sys.argv)
    if not os.path.exists('cache'):
        os.makedirs('cache')
    QApplication.processEvents()
    tool = ScreenshotTool()
    tool.show()
    sys.exit(app.exec_())