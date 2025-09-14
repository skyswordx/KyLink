import sys
from PyQt5.QtWidgets import QWidget, QApplication
from PyQt5.QtGui import QPainter, QPen, QBrush, QPixmap, QGuiApplication, QColor
from PyQt5.QtCore import Qt, QRect, pyqtSignal

# 尝试导入Pillow，如果失败则提示
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
        if not ImageGrab: # 仍然保留Pillow作为可用性检查
            self.close()
            return

        # 获取屏幕几何信息并设置窗口
        self.screen = QGuiApplication.primaryScreen()
        self.setGeometry(self.screen.geometry())

        # 设置窗口属性：无边框，总在最前
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        self.setCursor(Qt.CrossCursor)

        self.begin = None
        self.end = None
        
        # 在窗口显示前，先捕获一次全屏背景
        self.background = self.screen.grabWindow(QApplication.desktop().winId())

    def paintEvent(self, event):
        """
        绘制事件
        """
        painter = QPainter(self)
        
        # 1. 绘制全屏背景截图
        painter.drawPixmap(self.rect(), self.background)
        
        # 2. 绘制半透明遮罩
        painter.setBrush(QBrush(QColor(0, 0, 0, 128))) # 使用QColor设置带alpha通道的黑色
        painter.drawRect(self.rect())
        
        # 3. 如果有选区，则在该区域绘制清晰的原始图像，并添加边框
        if self.begin and self.end:
            selection_rect = QRect(self.begin, self.end).normalized()
            
            # 从原始背景中提取选区部分
            selected_pixmap = self.background.copy(selection_rect)
            
            # 将提取的部分绘制到当前画布上，从而覆盖半透明遮罩
            painter.drawPixmap(selection_rect.topLeft(), selected_pixmap)
            
            # 绘制选区边框
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
        
        # 从已经存在的背景截图中裁剪出选区
        final_screenshot = self.background.copy(selection_rect)
        
        filepath = "temp_screenshot.png"
        final_screenshot.save(filepath)
        print(f"截图已保存到: {filepath}")
        
        # 发射信号
        self.screenshot_taken.emit(filepath)

# 独立运行测试
if __name__ == '__main__':
    app = QApplication(sys.argv)
    # 延迟一点时间启动，确保桌面内容已经准备好
    QApplication.processEvents()
    tool = ScreenshotTool()
    tool.show()
    sys.exit(app.exec_())