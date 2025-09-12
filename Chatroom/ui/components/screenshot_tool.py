import sys
from PyQt5.QtWidgets import QWidget, QApplication
from PyQt5.QtGui import QPainter, QPen, QBrush, QPixmap, QGuiApplication
from PyQt5.QtCore import Qt, QRect, pyqtSignal

# 尝试导入Pillow，如果失败则提示
try:
    from PIL import ImageGrab
except ImportError:
    ImageGrab = None
    print("警告：Pillow库未安装，截图功能不可用。请运行 'pip install Pillow'")

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
            
        # 获取屏幕几何信息
        screen = QGuiApplication.primaryScreen()
        self.setGeometry(screen.geometry())
        
        # 设置窗口属性：无边框，总在最前
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint)
        self.setCursor(Qt.CrossCursor)
        
        self.begin = None
        self.end = None

    def paintEvent(self, event):
        """
        绘制事件
        """
        painter = QPainter(self)
        
        # 绘制半透明黑色背景
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(Qt.black))
        # 创建一个带有透明度的区域
        overlay = QWidget.rect(self)
        painter.drawRect(overlay)
        painter.setOpacity(0.5)
        
        if self.begin and self.end:
            # 绘制选区（使其变得不透明）
            selection_rect = QRect(self.begin, self.end).normalized()
            painter.setOpacity(0) # 完全透明，显示下面的屏幕
            painter.drawRect(selection_rect)
            painter.setOpacity(1) # 恢复不透明
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
        
        # 将窗口坐标转换为屏幕坐标
        p1 = self.mapToGlobal(selection_rect.topLeft())
        p2 = self.mapToGlobal(selection_rect.bottomRight())
        
        # 使用Pillow进行截图
        # bbox需要(left, top, right, bottom)
        bbox = (p1.x(), p1.y(), p2.x(), p2.y())
        img = ImageGrab.grab(bbox=bbox, all_screens=True)
        
        # 保存到临时文件
        # 在实际应用中，你可能需要一个更好的临时文件管理策略
        filepath = "temp_screenshot.png"
        img.save(filepath)
        print(f"截图已保存到: {filepath}")
        
        # 发射信号
        self.screenshot_taken.emit(filepath)

# 独立运行测试
if __name__ == '__main__':
    app = QApplication(sys.argv)
    tool = ScreenshotTool()
    tool.show()
    sys.exit(app.exec_())
