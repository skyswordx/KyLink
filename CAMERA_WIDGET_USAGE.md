# 摄像头控件使用说明

## 概述

基于 PyQt Fluent Widgets 范式开发的摄像头控件，针对资源受限平台（RK3566）进行了优化。

## 特性

### ✅ 跨平台支持
- 使用 OpenCV 进行摄像头访问
- 支持 Windows、Linux、macOS
- 自动检测可用摄像头

### ✅ 资源优化（针对 RK3566）
- **降低帧率**：预览帧率 15fps（可配置）
- **降低分辨率**：默认 640x480（可配置）
- **JPEG 压缩**：保存质量 85%（平衡大小和质量）
- **独立线程**：摄像头捕获在独立线程，不阻塞 UI
- **内存管理**：及时释放摄像头资源

### ✅ Fluent Design 风格
- 使用 CardWidget 作为基础组件
- 遵循 Fluent Design 视觉规范
- 支持暗色/亮色主题自适应

## 接口说明

### CameraDialog
主要的摄像头对话框，用于在聊天窗口中打开摄像头。

#### 信号
- `photo_captured(str)`: 拍照完成信号，参数为保存的图片路径

#### 使用示例
```python
from ui.components.camera_widget import CameraDialog

dialog = CameraDialog(parent)
dialog.photo_captured.connect(self.on_photo_captured)
dialog.exec_()
```

### CameraPreviewWidget
摄像头预览控件，可以独立使用或嵌入到其他界面。

#### 信号
- `frame_captured(np.ndarray)`: 帧捕获信号，参数为原始帧数据（numpy array）
- `capture_finished(str)`: 捕获完成信号，参数为保存路径

#### 方法
- `start_camera(camera_index=0)`: 启动摄像头
- `stop_camera()`: 停止摄像头
- `capture_frame(save_path=None)`: 捕获当前帧并保存

#### 使用示例
```python
from ui.components.camera_widget import CameraPreviewWidget

preview = CameraPreviewWidget()
preview.start_camera(0)  # 启动第一个摄像头
preview.capture_frame("photo.jpg")  # 拍照
preview.stop_camera()  # 停止摄像头
```

### get_available_cameras()
获取可用摄像头列表。

#### 返回值
`list`: `[(index, name), ...]` 摄像头索引和名称的列表

#### 使用示例
```python
from ui.components.camera_widget import get_available_cameras

cameras = get_available_cameras()
for index, name in cameras:
    print(f"摄像头 {index}: {name}")
```

## 配置参数

### 性能优化参数（可在 CameraPreviewWidget 中调整）

```python
# 预览帧率（降低以节省CPU）
self.preview_fps = 15  # 默认 15fps

# 捕获分辨率（降低以节省内存和CPU）
self.capture_resolution = (640, 480)  # 默认 640x480

# 预览分辨率（降低以节省GPU）
self.preview_resolution = (640, 480)  # 默认 640x480
```

### 针对不同平台的推荐配置

#### RK3566 (ARM, 资源受限)
```python
preview_fps = 10  # 降低到 10fps
capture_resolution = (640, 480)  # 640x480
preview_resolution = (640, 480)
```

#### 桌面平台 (性能充足)
```python
preview_fps = 30  # 可以提高到 30fps
capture_resolution = (1280, 720)  # 720p
preview_resolution = (1280, 720)
```

## 依赖要求

### 必需依赖
```bash
pip install opencv-python
pip install numpy
```

### 已包含依赖
- PyQt5 (已在项目中)
- qfluentwidgets (已在项目中)
- qframelesswindow (已在项目中)

## 集成到聊天窗口

已经集成到 `ChatWindow` 中：

1. **添加摄像头按钮**：在工具栏中添加了摄像头图标按钮
2. **打开摄像头对话框**：点击按钮打开 `CameraDialog`
3. **自动发送照片**：拍照后自动发送给对方

## 资源占用估算

### 内存占用
- **摄像头缓冲区**: ~1-2 MB (640x480, BGR)
- **预览缓冲区**: ~1-2 MB (RGB)
- **OpenCV 库**: ~5-10 MB (共享库)
- **总计**: ~7-14 MB

### CPU 占用
- **15fps 预览**: ~5-10% (RK3566)
- **10fps 预览**: ~3-5% (RK3566)
- **图像编码**: ~1-2% (拍照时)

### 磁盘占用
- **OpenCV**: ~50-100 MB (系统库)
- **照片大小**: ~50-200 KB/张 (JPEG 85% 质量)

## 错误处理

控件会自动处理以下错误：
- 摄像头打开失败
- 摄像头读取失败
- 文件保存失败

错误信息会通过信号或 MessageBox 显示。

## 扩展接口

### 自定义摄像头参数

```python
class CustomCameraDialog(CameraDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        # 自定义配置
        self.camera_preview.preview_fps = 20
        self.camera_preview.capture_resolution = (1280, 720)
```

### 实时帧处理

```python
preview = CameraPreviewWidget()
preview.frame_captured.connect(self.process_frame)

def process_frame(self, frame):
    # frame 是 numpy array (BGR格式)
    # 可以进行实时处理，如：
    # - 人脸检测
    # - 滤镜效果
    # - 图像增强
    pass
```

### 视频录制（扩展）

```python
class VideoRecorder:
    def __init__(self, camera_preview):
        self.camera_preview = camera_preview
        self.frames = []
        self.recording = False
    
    def start_recording(self):
        self.recording = True
        self.camera_preview.frame_captured.connect(self.save_frame)
    
    def save_frame(self, frame):
        if self.recording:
            self.frames.append(frame)
    
    def stop_recording(self, output_path):
        self.recording = False
        # 使用 OpenCV VideoWriter 保存视频
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        out = cv2.VideoWriter(output_path, fourcc, 15.0, (640, 480))
        for frame in self.frames:
            out.write(frame)
        out.release()
```

## 故障排除

### 问题：无法打开摄像头

**解决方案**：
1. 检查摄像头是否被其他程序占用
2. 检查摄像头权限（Linux 需要用户组权限）
3. 尝试不同的摄像头索引（0, 1, 2...）

### 问题：性能问题

**解决方案**：
1. 降低帧率：`preview_fps = 10`
2. 降低分辨率：`capture_resolution = (320, 240)`
3. 检查是否有其他进程占用 CPU

### 问题：内存占用过高

**解决方案**：
1. 降低预览分辨率
2. 及时停止摄像头：`stop_camera()`
3. 确保关闭对话框时自动停止摄像头

## 最佳实践

1. **及时释放资源**：关闭对话框时自动停止摄像头
2. **错误处理**：始终捕获异常并提示用户
3. **性能监控**：在资源受限平台上监控 CPU 和内存使用
4. **用户体验**：显示加载状态和错误提示

## 许可证

与主项目保持一致（GPLv3）

