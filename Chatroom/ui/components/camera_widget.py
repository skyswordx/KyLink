# coding:utf-8
"""
摄像头控件 - 基于 PyQt Fluent Widgets 范式
针对资源受限平台优化（RK3566）
"""
import os
import cv2
import numpy as np
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QLabel, 
                             QPushButton, QSizePolicy)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal, QThread, QSize, QRect
from PyQt5.QtGui import QImage, QPixmap, QPainter, QColor, QPen, QBrush

from qfluentwidgets import (PushButton, ToolButton, CardWidget, 
                           FluentIcon as FIF, isDarkTheme, InfoBar)
from qframelesswindow import FramelessDialog, StandardTitleBar


class CameraCaptureThread(QThread):
    """
    摄像头捕获线程 - 独立线程避免阻塞UI
    优化：降低帧率以节省CPU和内存
    """
    frame_ready = pyqtSignal(np.ndarray)  # 原始帧数据
    error_occurred = pyqtSignal(str)      # 错误信息
    
    def __init__(self, camera_index=0, fps=15, resolution=(640, 480)):
        super().__init__()
        self.camera_index = camera_index
        self.fps = fps
        self.resolution = resolution
        self.running = False
        self.cap = None
        
    def run(self):
        """线程主循环"""
        try:
            # 打开摄像头
            self.cap = cv2.VideoCapture(self.camera_index)
            if not self.cap.isOpened():
                self.error_occurred.emit(f"无法打开摄像头 {self.camera_index}")
                return
            
            # 设置分辨率（降低分辨率以节省资源）
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.resolution[0])
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.resolution[1])
            # 设置帧率（降低帧率以节省CPU）
            self.cap.set(cv2.CAP_PROP_FPS, self.fps)
            
            # 帧间隔（毫秒）
            frame_interval = int(1000 / self.fps)
            
            while self.running:
                ret, frame = self.cap.read()
                if ret:
                    # 发射帧数据信号
                    self.frame_ready.emit(frame)
                else:
                    self.error_occurred.emit("读取摄像头帧失败")
                    break
                
                # 控制帧率
                self.msleep(frame_interval)
                
        except Exception as e:
            self.error_occurred.emit(f"摄像头错误: {str(e)}")
        finally:
            if self.cap:
                self.cap.release()
    
    def stop(self):
        """停止捕获"""
        self.running = False
        self.wait(1000)  # 等待最多1秒


class CameraPreviewWidget(CardWidget):
    """
    摄像头预览控件 - Fluent Design 风格
    优化：使用硬件加速、降低分辨率
    """
    # 信号定义
    frame_captured = pyqtSignal(np.ndarray)  # 捕获的帧
    capture_finished = pyqtSignal(str)        # 捕获完成，参数为保存路径
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("CameraPreviewWidget")
        
        # 摄像头相关
        self.camera_thread = None
        self.current_frame = None
        self.is_capturing = False
        
        # 优化参数（针对RK3566）
        self.preview_fps = 15  # 预览帧率（降低以节省CPU）
        self.capture_resolution = (640, 480)  # 捕获分辨率
        self.preview_resolution = (640, 480)   # 预览分辨率
        
        # UI 设置
        self.setMinimumSize(640, 480)
        self.setMaximumSize(1280, 720)  # 限制最大尺寸
        
        self.init_ui()
        self.setup_style()
    
    def init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        
        # 预览标签（显示摄像头画面）
        self.preview_label = QLabel(self)
        self.preview_label.setAlignment(Qt.AlignCenter)
        self.preview_label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.preview_label.setText("摄像头未启动")
        self.preview_label.setStyleSheet("""
            QLabel {
                background-color: rgba(0, 0, 0, 0.8);
                color: white;
                border-radius: 4px;
                font-size: 16px;
            }
        """)
        
        layout.addWidget(self.preview_label)
    
    def setup_style(self):
        """设置样式"""
        if isDarkTheme():
            bg_color = "rgb(43, 43, 43)"
        else:
            bg_color = "rgb(255, 255, 255)"
        
        self.setStyleSheet(f"""
            CameraPreviewWidget {{
                background-color: {bg_color};
                border: 1px solid rgba(0, 0, 0, 0.1);
                border-radius: 8px;
            }}
        """)
    
    def start_camera(self, camera_index=0):
        """启动摄像头"""
        if self.camera_thread and self.camera_thread.isRunning():
            return
        
        self.camera_thread = CameraCaptureThread(
            camera_index=camera_index,
            fps=self.preview_fps,
            resolution=self.capture_resolution
        )
        self.camera_thread.frame_ready.connect(self.on_frame_received)
        self.camera_thread.error_occurred.connect(self.on_camera_error)
        self.camera_thread.running = True
        self.camera_thread.start()
        
        self.preview_label.setText("正在启动摄像头...")
    
    def stop_camera(self):
        """停止摄像头"""
        if self.camera_thread:
            self.camera_thread.stop()
            self.camera_thread = None
        
        self.current_frame = None
        self.preview_label.clear()
        self.preview_label.setText("摄像头已关闭")
        self.update()
    
    def on_frame_received(self, frame):
        """处理接收到的帧"""
        self.current_frame = frame
        
        # 调整预览尺寸（降低分辨率以节省资源）
        preview_frame = cv2.resize(frame, self.preview_resolution)
        
        # 转换为QImage（优化：使用RGB格式）
        rgb_frame = cv2.cvtColor(preview_frame, cv2.COLOR_BGR2RGB)
        h, w, ch = rgb_frame.shape
        bytes_per_line = ch * w
        qt_image = QImage(rgb_frame.data, w, h, bytes_per_line, QImage.Format_RGB888)
        
        # 转换为QPixmap并显示
        pixmap = QPixmap.fromImage(qt_image)
        
        # 缩放以适应标签大小（保持宽高比）
        scaled_pixmap = pixmap.scaled(
            self.preview_label.size(),
            Qt.KeepAspectRatio,
            Qt.SmoothTransformation
        )
        
        self.preview_label.setPixmap(scaled_pixmap)
    
    def on_camera_error(self, error_msg):
        """处理摄像头错误"""
        self.preview_label.setText(f"摄像头错误: {error_msg}")
        self.stop_camera()
    
    def capture_frame(self, save_path=None):
        """捕获当前帧"""
        if self.current_frame is None:
            return None
        
        # 如果没有指定路径，生成临时路径
        if save_path is None:
            import time
            timestamp = int(time.time() * 1000)
            os.makedirs("cache", exist_ok=True)
            save_path = os.path.join("cache", f"camera_{timestamp}.jpg")
        
        # 保存图像（使用JPEG压缩以节省空间）
        cv2.imwrite(save_path, self.current_frame, 
                   [cv2.IMWRITE_JPEG_QUALITY, 85])  # 85%质量，平衡大小和质量
        
        self.frame_captured.emit(self.current_frame)
        self.capture_finished.emit(save_path)
        
        return save_path
    
    def resizeEvent(self, event):
        """窗口大小改变时更新预览"""
        super().resizeEvent(event)
        if self.current_frame is not None:
            # 重新显示当前帧以适应新尺寸
            self.on_frame_received(self.current_frame)
    
    def closeEvent(self, event):
        """关闭时停止摄像头"""
        self.stop_camera()
        super().closeEvent(event)


class CameraDialog(FramelessDialog):
    """
    摄像头对话框 - Fluent Design 风格
    在聊天窗口中打开摄像头进行拍照
    """
    photo_captured = pyqtSignal(str)  # 拍照完成，参数为图片路径
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setTitleBar(StandardTitleBar(self))
        self.titleBar.raise_()
        self.setWindowTitle("摄像头")
        self.setMinimumSize(680, 580)
        
        # 摄像头预览控件
        self.camera_preview = None
        
        self.init_ui()
        self.setup_style()
    
    def init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, self.titleBar.height(), 0, 0)
        
        content_widget = QWidget()
        content_layout = QVBoxLayout(content_widget)
        content_layout.setContentsMargins(15, 15, 15, 15)
        content_layout.setSpacing(15)
        
        # 摄像头预览
        self.camera_preview = CameraPreviewWidget(self)
        self.camera_preview.frame_captured.connect(self.on_frame_captured)
        self.camera_preview.capture_finished.connect(self.on_capture_finished)
        
        # 控制按钮
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        self.capture_button = PushButton("拍照", self)
        self.capture_button.setIcon(FIF.CAMERA)
        self.capture_button.clicked.connect(self.capture_photo)
        
        self.close_button = PushButton("关闭", self)
        self.close_button.clicked.connect(self.close)
        
        button_layout.addWidget(self.close_button)
        button_layout.addWidget(self.capture_button)
        
        content_layout.addWidget(self.camera_preview)
        content_layout.addLayout(button_layout)
        
        layout.addWidget(content_widget)
        
        # 启动摄像头
        self.camera_preview.start_camera()
    
    def setup_style(self):
        """设置样式"""
        if isDarkTheme():
            window_bg = "rgb(32, 32, 32)"
            widget_bg = "rgb(43, 43, 43)"
        else:
            window_bg = "rgb(243, 243, 243)"
            widget_bg = "rgb(255, 255, 255)"
        
        self.setStyleSheet(f"""
            FramelessDialog {{
                background-color: {window_bg};
            }}
            QWidget {{
                background-color: {widget_bg};
            }}
        """)
    
    def capture_photo(self):
        """拍照"""
        if self.camera_preview:
            self.capture_button.setEnabled(False)
            self.capture_button.setText("正在保存...")
            self.camera_preview.capture_frame()
    
    def on_frame_captured(self, frame):
        """帧捕获回调"""
        pass
    
    def on_capture_finished(self, save_path):
        """拍照完成回调"""
        self.capture_button.setEnabled(True)
        self.capture_button.setText("拍照")
        
        # 发射信号
        self.photo_captured.emit(save_path)
        
        # 可选：显示成功提示
        InfoBar.success(
            title="拍照成功",
            content="照片已保存",
            duration=2000,
            parent=self
        )
    
    def closeEvent(self, event):
        """关闭对话框"""
        if self.camera_preview:
            self.camera_preview.stop_camera()
        super().closeEvent(event)


def get_available_cameras():
    """
    获取可用的摄像头列表
    返回: [(index, name), ...]
    """
    cameras = []
    # 尝试检测最多5个摄像头
    for i in range(5):
        cap = cv2.VideoCapture(i)
        if cap.isOpened():
            # 尝试获取摄像头名称（某些系统可能不支持）
            try:
                name = f"摄像头 {i}"
                cameras.append((i, name))
            except:
                cameras.append((i, f"摄像头 {i}"))
            cap.release()
    return cameras

