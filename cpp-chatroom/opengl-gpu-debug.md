## 系统状态打表 1

```bash
FHD Camera: FHD Camera (usb-fd800000.usb-1):
	/dev/video10
	/dev/video11
	/dev/media1

kylin@kylin:~/Desktop$ v4l2-ctl -d /dev/video10  --list-formats-ext
ioctl: VIDIOC_ENUM_FMT
	Type: Video Capture

	[0]: 'MJPG' (Motion-JPEG, compressed)
		Size: Discrete 1920x1080
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 1280x720
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 640x480
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 640x360
			Interval: Discrete 0.033s (30.000 fps)
	[1]: 'YUYV' (YUYV 4:2:2)
		Size: Discrete 640x480
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 640x360
			Interval: Discrete 0.033s (30.000 fps)
	[2]: 'H264' (H.264, compressed)
		Size: Discrete 1920x1080
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 1280x720
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 640x480
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 640x360
			Interval: Discrete 0.033s (30.000 fps)
	[3]: 'HEVC' (HEVC, compressed)
		Size: Discrete 1920x1080
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 1280x720
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 640x480
			Interval: Discrete 0.033s (30.000 fps)
		Size: Discrete 640x360
			Interval: Discrete 0.033s (30.000 fps)
kylin@kylin:~/Desktop$ v4l2-ctl -d /dev/video11  --list-formats-ext
ioctl: VIDIOC_ENUM_FMT
	Type: Video Capture


kylin@kylin:~/Desktop$ ldd FeiQChatroom 
	linux-vdso.so.1 (0x0000007f8c4d1000)
	libQt5MultimediaWidgets.so.5 => /lib/aarch64-linux-gnu/libQt5MultimediaWidgets.so.5 (0x0000007f8c3d8000)
	libQt5Widgets.so.5 => /lib/aarch64-linux-gnu/libQt5Widgets.so.5 (0x0000007f8bd5d000)
	libQt5Multimedia.so.5 => /lib/aarch64-linux-gnu/libQt5Multimedia.so.5 (0x0000007f8bc3d000)
	libQt5Gui.so.5 => /lib/aarch64-linux-gnu/libQt5Gui.so.5 (0x0000007f8b6b5000)
	libQt5Network.so.5 => /lib/aarch64-linux-gnu/libQt5Network.so.5 (0x0000007f8b4e9000)
	libQt5Core.so.5 => /lib/aarch64-linux-gnu/libQt5Core.so.5 (0x0000007f8afb9000)
	libpthread.so.0 => /lib/aarch64-linux-gnu/libpthread.so.0 (0x0000007f8af89000)
	libstdc++.so.6 => /lib/aarch64-linux-gnu/libstdc++.so.6 (0x0000007f8ada4000)
	libc.so.6 => /lib/aarch64-linux-gnu/libc.so.6 (0x0000007f8ac31000)
	/lib/ld-linux-aarch64.so.1 (0x0000007f8c4a1000)
	libgcc_s.so.1 => /lib/aarch64-linux-gnu/libgcc_s.so.1 (0x0000007f8ac0d000)
	libQt5OpenGL.so.5 => /lib/aarch64-linux-gnu/libQt5OpenGL.so.5 (0x0000007f8aba8000)
	libGL.so.1 => /lib/aarch64-linux-gnu/libGL.so.1 (0x0000007f8aab1000)
	libm.so.6 => /lib/aarch64-linux-gnu/libm.so.6 (0x0000007f8aa04000)
	libpulse.so.0 => /lib/aarch64-linux-gnu/libpulse.so.0 (0x0000007f8a9a9000)
	libpng16.so.16 => /lib/aarch64-linux-gnu/libpng16.so.16 (0x0000007f8a965000)
	libz.so.1 => /lib/aarch64-linux-gnu/libz.so.1 (0x0000007f8a93b000)
	libharfbuzz.so.0 => /lib/aarch64-linux-gnu/libharfbuzz.so.0 (0x0000007f8a83c000)
	libdl.so.2 => /lib/aarch64-linux-gnu/libdl.so.2 (0x0000007f8a828000)
	libicui18n.so.66 => /lib/aarch64-linux-gnu/libicui18n.so.66 (0x0000007f8a53c000)
	libicuuc.so.66 => /lib/aarch64-linux-gnu/libicuuc.so.66 (0x0000007f8a34f000)
	libpcre2-16.so.0 => /lib/aarch64-linux-gnu/libpcre2-16.so.0 (0x0000007f8a2ca000)
	libdouble-conversion.so.3 => /lib/aarch64-linux-gnu/libdouble-conversion.so.3 (0x0000007f8a2a7000)
	libglib-2.0.so.0 => /lib/aarch64-linux-gnu/libglib-2.0.so.0 (0x0000007f8a16d000)
	libGLdispatch.so.0 => /lib/aarch64-linux-gnu/libGLdispatch.so.0 (0x0000007f89fe2000)
	libGLX.so.0 => /lib/aarch64-linux-gnu/libGLX.so.0 (0x0000007f89fa0000)
	libpulsecommon-13.99.so => /usr/lib/aarch64-linux-gnu/pulseaudio/libpulsecommon-13.99.so (0x0000007f89f1a000)
	libdbus-1.so.3 => /lib/aarch64-linux-gnu/libdbus-1.so.3 (0x0000007f89ebb000)
	libfreetype.so.6 => /lib/aarch64-linux-gnu/libfreetype.so.6 (0x0000007f89dfb000)
	libgraphite2.so.3 => /lib/aarch64-linux-gnu/libgraphite2.so.3 (0x0000007f89dc9000)
	libicudata.so.66 => /lib/aarch64-linux-gnu/libicudata.so.66 (0x0000007f882fa000)
	libpcre.so.3 => /lib/aarch64-linux-gnu/libpcre.so.3 (0x0000007f88288000)
	libX11.so.6 => /lib/aarch64-linux-gnu/libX11.so.6 (0x0000007f88143000)
	libxcb.so.1 => /lib/aarch64-linux-gnu/libxcb.so.1 (0x0000007f8810c000)
	libsystemd.so.0 => /lib/aarch64-linux-gnu/libsystemd.so.0 (0x0000007f8804e000)
	libwrap.so.0 => /lib/aarch64-linux-gnu/libwrap.so.0 (0x0000007f88034000)
	libsndfile.so.1 => /lib/aarch64-linux-gnu/libsndfile.so.1 (0x0000007f87fad000)
	libasyncns.so.0 => /lib/aarch64-linux-gnu/libasyncns.so.0 (0x0000007f87f97000)
	libapparmor.so.1 => /lib/aarch64-linux-gnu/libapparmor.so.1 (0x0000007f87f75000)
	librt.so.1 => /lib/aarch64-linux-gnu/librt.so.1 (0x0000007f87f5d000)
	libXau.so.6 => /lib/aarch64-linux-gnu/libXau.so.6 (0x0000007f87f47000)
	libXdmcp.so.6 => /lib/aarch64-linux-gnu/libXdmcp.so.6 (0x0000007f87f31000)
	liblzma.so.5 => /lib/aarch64-linux-gnu/liblzma.so.5 (0x0000007f87efd000)
	liblz4.so.1 => /lib/aarch64-linux-gnu/liblz4.so.1 (0x0000007f87ecf000)
	libgcrypt.so.20 => /lib/aarch64-linux-gnu/libgcrypt.so.20 (0x0000007f87e03000)
	libnsl.so.1 => /lib/aarch64-linux-gnu/libnsl.so.1 (0x0000007f87dd8000)
	libFLAC.so.8 => /lib/aarch64-linux-gnu/libFLAC.so.8 (0x0000007f87d95000)
	libogg.so.0 => /lib/aarch64-linux-gnu/libogg.so.0 (0x0000007f87d7b000)
	libvorbis.so.0 => /lib/aarch64-linux-gnu/libvorbis.so.0 (0x0000007f87d42000)
	libvorbisenc.so.2 => /lib/aarch64-linux-gnu/libvorbisenc.so.2 (0x0000007f87c92000)
	libresolv.so.2 => /lib/aarch64-linux-gnu/libresolv.so.2 (0x0000007f87c6a000)
	libbsd.so.0 => /lib/aarch64-linux-gnu/libbsd.so.0 (0x0000007f87c43000)
	libgpg-error.so.0 => /lib/aarch64-linux-gnu/libgpg-error.so.0 (0x0000007f87c13000)
kylin@kylin:~/Desktop$ 




sudo apt-get install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libv4l-dev v4l-utils libopenblas-dev libgtk2.0-dev libavcodec-dev libavformat-dev libswscale-dev libtbb2 libtbb-dev libjpeg-dev libpng-dev libtiff-dev libdc1394-22-dev


kylin@kylin:~/Desktop$ GST_DEBUG=3 gst-launch-1.0 v4l2src device=/dev/video10 ! videoconvert ! autovideosink
Setting pipeline to PAUSED ...
Pipeline is live and does not need PREROLL ...
Setting pipeline to PLAYING ...
New clock: GstSystemClock
0:00:00.964129763 20686   0x55a1a8fa40 WARN                 v4l2src gstv4l2src.c:978:gst_v4l2src_create:<v4l2src0> lost frames detected: count = 2 - ts: 0:00:00.646451268



kylin@kylin:~/Desktop$ ./FeiQChatroom 
libGL error: failed to create dri screen
libGL error: failed to load driver: rockchip
libGL error: failed to create dri screen
libGL error: failed to load driver: rockchip
Unable to query the parameter info: QCameraImageProcessingControl::WhiteBalancePreset : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::ColorTemperature : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::ContrastAdjustment : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::SaturationAdjustment : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::BrightnessAdjustment : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::SharpeningAdjustment : "Invalid argument"
mpp[20535]: mpp_rt: NOT found ion allocator
mpp[20535]: mpp_rt: found drm allocator
mpp[20535]: mpp_info: mpp version: 49f29006 author: Jeffy Chen    2021-08-04 [drm]: Add mmap flag detection
mpp[20535]: mpp_info: mpp version: 49f29006 author: Jeffy Chen    2021-08-04 [drm]: Add mmap flag detection
CameraBin error: "Failed to allocate required memory."
Unable to query the parameter info: QCameraImageProcessingControl::WhiteBalancePreset : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::ColorTemperature : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::ContrastAdjustment : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::SaturationAdjustment : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::BrightnessAdjustment : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::SharpeningAdjustment : "Invalid argument"
mpp[20535]: mpp_info: mpp version: 49f29006 author: Jeffy Chen    2021-08-04 [drm]: Add mmap flag detection
mpp[20535]: mpp_info: mpp version: 49f29006 author: Jeffy Chen    2021-08-04 [drm]: Add mmap flag detection
QWidget::paintEngine: Should no longer be called
QWidget::paintEngine: Should no longer be called
QWidget::paintEngine: Should no longer be called
QWidget::paintEngine: Should no longer be called
Unable to query the parameter info: QCameraImageProcessingControl::WhiteBalancePreset : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::ColorTemperature : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::ContrastAdjustment : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::SaturationAdjustment : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::BrightnessAdjustment : "Invalid argument"
Unable to query the parameter info: QCameraImageProcessingControl::SharpeningAdjustment : "Invalid argument"

```
## 博客


### 👤 linzeshi

**2024年12月26日**

**产品：** LPB3588
**系统：** LPB3588\_ubuntu20.04\_v1.0\_20241010\_1726.img
**问题描述：**

1.  在可视化界面打开软件时，会有如下警告

    ```
    libGL error: failed to create dri screen
    libGL error: failed to load driver: rockchip
    ```

    虽然也能打开软件，但是整个系统会变得卡卡的，CPU占用率变高。感觉像是找不到GPU驱动相关的库，就用CPU解码了？

-----

### 👤 linzeshi

**2024年12月26日**

完整的警告和错误如下

```
libGL warning: DRI2: failed to create dri screen
libGL error: failed to create dri screen
libGL error: failed to load driver: rockchip
```

-----

### 👤 neardi

**2024年12月26日**

  * 1、具体是打开什么软件？

  * 2、打开 glmark2-es2 是什么结果

  * 3、执行以下命令后再试试

    ```
    export LIBGL_ALWAYS_SOFTWARE=1
    export LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/
    ```

## 分析

我和这个博客的记录发生的错误很像，于是我使用了glmark进行测试，使用glmark测试到一半rk3566就重启了，我之前发现重启的情况是内存占用 过多，或者磁盘空间不足，我后面部署了你更新后的程序，使用了export的 soft的方法，但还是出现了类似文档中的报错


- 你遇到的报错与那篇博客描述的 GPU/DRI 问题本质一致：`glmark2` 跑到一半直接把板子重启，说明内核里的 Rockchip GPU 驱动（或固件、电源、内存 allocator）在初始化/跑分时崩掉。Qt/cheese 在初始化 OpenGL/EGL 时同样找不到可用的 Rockchip DRI 驱动，于是回退到软渲染或直接失败，这就是最初看到 `libGL error: failed to load driver: rockchip` 的原因。虽然你现在导出了软件渲染的环境变量，确实绕开了 `libGL error`，但根因（GPU 驱动不稳定、显存分配失败）仍然存在。  

- `Unable to query the parameter info …` 来自 RK ISP/UVC 驱动——它只是告诉你这些图像处理控件未实现，不影响预览；真正阻塞预览的是底层显存/图形栈。`QWidget::paintEngine: Should no longer be called` 说明 QtMultimedia 的默认 qtvideosink 仍然尝试通过 OpenGL PaintEngine 绘制帧，但在软渲染/驱动异常下没有拿到有效的 GL 上下文。

- 建议把系统层面的 GPU/显存问题排查清楚：  
  1. 确认你刷的是带 GPU 驱动的完整固件（比如 Firefly/厂商发布的最新版）。如果自己裁剪过系统，检查 rockchip_dri.so、`/usr/lib/aarch64-linux-gnu/libmali-...so` 是否存在。  
  2. 检查 card0、renderD128、`/dev/mpp_service` 权限，当前用户需在 `video` 组。  
  3. 若有 Rockchip 提供的 GPU 安装脚本（如 `install_libmali.sh`），重新执行一次，随后 `sudo ldconfig`。  
  4. 若 glmark2 仍导致重启，多半是 GPU 驱动或电源管理的已知 bug，需要更新到厂商修复版内核/固件，或暂时禁用 GPU（`echo performance > /sys/devices/platform/ff9a0000.gpu/devfreq/ff9a0000.gpu/governor` 等方式可能缓解，但治标不治本）。

- 在彻底修好 GPU 前，可以继续使用纯软件路径自检：  
  - 运行前导出：  
    ```
    export QT_QPA_PLATFORM=xcb
    export QT_OPENGL=software
    export LIBGL_ALWAYS_SOFTWARE=1
    export GST_GL_PLATFORM=mesa
    ```  
    让 Qt 和 GStreamer 都走 CPU 渲染；这路径下如果仍然没有图像，那就是 GPU 之外的问题。  
  - 保持使用 `FHD Camera` 这类真正的 V4L2 节点；`rkisp_mainpath`、`rawwr*` 是 ISP 内部设备，不适合作为直接预览源。  
  - 在代码里你已经看到了 `handleCameraError`，可以临时在按钮旁加个状态栏或 `qDebug()` 输出 `m_camera->errorString()`，以确认 Qt 实际收到的错误类型；目前看到的只有驱动 capability 警告，说明 camerabin 管线其实跑起来了，只是渲染端拿不到帧。

- 总结：  
  1. 现象与博客一致，问题核心在 Rockchip GPU/显存驱动，先修系统环境。  
  2. 在系统完全恢复前，用软件渲染变量和 `gst-launch` 验证数据流，确保摄像头本身没问题。  
  3. 一旦 GPU 栈稳定（glmark2 不再崩，`libGL` 不再报错），Qt 的 `QCameraViewfinder` 才能恢复硬件加速渲染，届时这些 `paintEngine` 与参数警告自然会消失。    让 Qt 和 GStreamer 都走 CPU 渲染；这路径下如果仍然没有图像，那就是 GPU 之外的问题。  
  - 保持使用 `FHD Camera` 这类真正的 V4L2 节点；`rkisp_mainpath`、`rawwr*` 是 ISP 内部设备，不适合作为直接预览源。  
  - 在代码里你已经看到了 `handleCameraError`，可以临时在按钮旁加个状态栏或 `qDebug()` 输出 `m_camera->errorString()`，以确认 Qt 实际收到的错误类型；目前看到的只有驱动 capability 警告，说明 camerabin 管线其实跑起来了，只是渲染端拿不到帧。

- 总结：  
  1. 现象与博客一致，问题核心在 Rockchip GPU/显存驱动，先修系统环境。  
  2. 在系统完全恢复前，用软件渲染变量和 `gst-launch` 验证数据流，确保摄像头本身没问题。  
  3. 一旦 GPU 栈稳定（glmark2 不再崩，`libGL` 不再报错），Qt 的 `QCameraViewfinder` 才能恢复硬件加速渲染，届时这些 `paintEngine` 与参数警告自然会消失。


## 系统状态打表 2

```bash
# 0. 查看系统版本与内核
uname -a
cat /etc/os-release

# 1. 检查摄像头与 GPU 设备节点
ls /dev/video*
v4l2-ctl --list-devices
ls -l /dev/dri
ls -l /dev/mpp_service

# 2. 查看摄像头详细能力
v4l2-ctl --all -d /dev/video10
v4l2-ctl --list-formats-ext -d /dev/video10

# 3. 确认 gstreamer 插件/元素
gst-inspect-1.0 v4l2src
gst-inspect-1.0 videoconvert
gst-inspect-1.0 autovideosink
gst-inspect-1.0 glimagesink
gst-inspect-1.0 kmssink

# 4. 命令行预览测试（留意输出）
GST_DEBUG=3 gst-launch-1.0 v4l2src device=/dev/video10 ! videoconvert ! autovideosink

# 5. GPU/OpenGL 基线信息
glxinfo | head
glxinfo -B
es2_info

# 6. GPU 驱动库/固件检查
ls /usr/lib/aarch64-linux-gnu/dri | grep -E "rockchip|mali"
ls /usr/lib/aarch64-linux-gnu | grep -i mali
strings /var/log/Xorg.0.log | grep -i glamor

# 7. 内存与磁盘状态
free -h
df -h

# 8. 当前用户是否在 video 组
id
```

```bash

```

## 路线讨论

- Qt 5.12 在 Linux/嵌入式上默认用 **QtMultimedia + GStreamer camerabin**，最后的显示环节是 `qtvideosink`，它内部依赖 OpenGL（通过 `QVideoFrame` → GL texture → QWidget）。目前板子的 GL 驱动缺失，QtViewfinder 就会掉到软件路径而且频繁报 `paintEngine` 警告，这与 standalone `gst-launch ... autovideosink` 能出图的情况不同。  
- 要彻底避开 Qt 的 OpenGL，有两种思路：  
  1. **不用 `QCameraViewfinder`，自己拉帧再画。**  
     - 用 `QCamera` + `QVideoProbe` 或 `QCameraImageCapture` 拿到 `QVideoFrame`，`map()` 成 `QImage`，在 `QLabel`/`QWidget::paintEvent` 里用 CPU 绘制。  
     - 这样完全绕开 GL，但要自己做色彩转换（例如 MJPEG/H264 → YUYV → RGB），帧率会受 CPU 限制。为了减负要在 `QCameraViewfinderSettings` 中固定 `YUYV`/`640x480`。  
  2. **直接管理 GStreamer 管线，嵌入视频窗口。**  
     - 在 Qt 应用里用 GStreamer C API 创建与你测试时相同的 pipeline（例如 `v4l2src ! videoconvert ! autovideosink`），再通过 `gst_video_overlay_set_window_handle()` 把 autovideosink 的窗口句柄绑定到一个 Qt 子窗口上。  
     - 这种方案显示链条与命令行一致，可改用 `ximagesink`、`kmssink` 等非 GL sink。缺点是需要手写 GStreamer 控制逻辑，丢掉 QtMultimedia 的封装。  
- 其它途径（例如环境变量强行让 Qt 不用 OpenGL）在 Qt 5.12 中效果有限；Qt 官方没有提供“纯软件 viewfinder”配置。若不重写显示层，仍然会走 GL。  
- 因此，如果短期无法修复 Mali 驱动，又想继续在 Qt 内预览，可以考虑第 1 种方案（简单但 CPU 压力大）或第 2 种方案（工作量较大但性能和 gst-launch 接近）。不过从长期看，恢复 GPU 驱动仍是最佳选择，否则桌面和任何 Qt 动画都会持续占用大量 CPU。



- 是的，如果后续要把视频流送进 RKNN 做 NPU 推理，建议直接自己管理 GStreamer 管线。这样你可以在同一条管线里分叉出一个 `appsink` 或者 Rockchip 的专用插件，把原始帧以 DMA-BUF/CPU 内存的方式喂给 RKNN，同步再分叉到 `ximagesink/kmscube` 做预览。使用 QtMultimedia 时，这些底层元素都被封装住，很难拿到原始缓冲，而且默认还得先经过 OpenGL，既影响性能又不好做零拷贝。  
- 常见做法是：  
  ```
  v4l2src io-mode=dmabuf device=/dev/video10 !
      video/x-raw,format=NV12,width=640,height=480,framerate=30/1 !
      tee name=t
      t. ! queue ! kmssink       # 或者 ximagesink 嵌入 Qt 窗口
      t. ! queue ! appsink name=to_rknn sync=false max-buffers=1 drop=true
  ```
  `appsink` 回调里把 `GstSample` 取出，转换为 RKNN 需要的内存（支持时可以直接拿 DMA-BUF FD 给 `rknn_inputs_set`).  
- 在 Qt 里，你可以用 GStreamer C API 或 gst-rtsp-server，把上述管线嵌进 Qt 事件循环：  
  - `gst_parse_launch()` 创建管线；  
  - `gst_video_overlay_set_window_handle()` 把 `kmssink/ximagesink` 的窗口绑定到某个 Qt widget；  
  - 把 `appsink` 的回调结果投喂到 RKNN。  
- 这种方式让你完全控制采样格式、分辨率、同步策略，也最方便后续引入 RGA/NPU 等 Rockchip 平台加速；同时避免 QtMultimedia 的 OpenGL 依赖和封装限制，是更适合做推理的长线架构。


读取 [](file:///home/circlemoon/kylin/cpp-chatroom/RK3566%20YOLO%20NPU%20%E6%8E%A8%E7%90%86%E6%96%B9%E6%A1%88%E8%AF%84%E4%BC%B0.md)

直接自己管理 GStreamer 管线并把视频窗口嵌入 Qt，对后续接入 RKNN/NPU 推理是非常有利的，理由如下：

- **天然的数据通路**：GStreamer 管线可以把 `v4l2src` 采集到的帧同时分叉出两支，一支交给 `appsink` 或 Rockchip 的零拷贝插件（`rkispp`, `rkx11videosink` 等）送去预处理/NPU；另一支送到 `ximagesink/kmssink` 做显示。这样与方案评估中推荐的“单进程、零拷贝”原则完全一致，不需要再从 QtMultimedia 的 OpenGL 纹理回读到 CPU 再上传到 NPU。

- **匹配评估里“原生 C++ + RGA + RKNN”流水线**：在评估文档第 3 部分强调的数据流是“RGA 预处理 → RKNN 推理 → C++ 后处理”。自己掌握 GStreamer 后端时，可以在 pipeline 中直接插入 `rkximgproc`、`rkrga` 等加速 element，把图像在 GPU/ISP/RGA 上预处理好，再通过 `appsink` 取得 DMA-BUF/Fd 指针喂给 `rknn_inputs_set`，完全符合零拷贝路径。

- **易于扩展后处理**：QtMultimedia 的封装只给你渲染图像的接口，拿不到原始帧；使用 GStreamer API 时，你可以在 `appsink` 回调中直接触发 QThread 里的 RKNN 推理 worker（文档第 4 部分推荐的线程模型），推理出的检测框也能通过 Qt 信号槽回到 UI 层。

- **避免 OpenGL 依赖**：当前板子的 GPU 驱动不稳定，自建管线可以选用 `xvimagesink`、`ximagesink` 或 `kmssink` 等非 OpenGL sink，从根源上绕过 `QCameraViewfinder` 的 OpenGL 渲染限制，让界面更顺畅，同时不影响 NPU 数据链。

- **与 Rockchip 生态示例一致**：官方 `rknn_model_zoo`、Qengineering 的 RK3566 YOLO 示例都是“GStreamer/RGA 采集 + C++ 推理 + appsink 输出”结构；采用同样的管线，后续复用这些示例代码会更直接。

**结论**：在现有 GPU 环境未完全修复的情况下，提前把采集/显示从 QtMultimedia 切换到“自管 GStreamer 管线 + Qt 嵌窗/信号槽”不仅能立刻改善稳定性，还为后续 RKNN 推理提供了最干净的零拷贝入口，完全契合评估方案中主张的原生 C++ 统一流水线。下一步可着手搭建 `v4l2src ! tee ! queue ! ximagesink` + `queue ! appsink` 的测试管线，并在 appsink 回调里集成 RGA/RKNN 代码，逐步替换现有的 `QCameraViewfinder`。

- QtMultimedia 的 `QCameraViewfinder` 最终走的是 Qt 的 GL 渲染链：`QVideoFrame → qtvideosink → OpenGL`。它会通过 libGL/GLX 去加载 `rockchip_dri.so`，结果系统里当前的 libGL 仍指向 Mesa（llvmpipe），于是抛出 `failed to load driver: rockchip`，Qt 退回软件渲染，出现 `paintEngine` 警告，帧率和内存占用都很差。  
- 你用 `glxinfo -B` 已经印证这一点：`OpenGL renderer string: llvmpipe (LLVM 12.0.0)`，说明 Xorg/GLX 根本没把 Mali 驱动接管进来。只要 Qt 依赖 GL，就会撞到这个问题。  
- GStreamer 的 `gst-launch … ! videoconvert ! autovideosink` 之所以能出图，原因是 autovideosink 会检测环境并选择最稳妥的后端。在没有可用 GPU 的情况下，它回退到了 `ximagesink` 或 `xvimagesink` 这类纯 CPU/软件路径，实现方式是直接往 X11 窗口拷贝图像缓冲，不依赖 OpenGL，因此不会触发 `failed to load driver`。  
- 换句话说，两个栈走的是不同的“最后一公里”：  
  - Qt → GLX → (缺失的) libmali → 报错  
  - GStreamer → X11/Xv → CPU 内存 blit → 正常显示  
- 所以现象上看像是 “Qt 有问题、GStreamer 正常”，实质上是 OpenGL 驱动没有正确加载；凡是依赖 GLX/Mali 的应用（Qt、glmark2、带 OpenGL 的浏览器）都会出同样的错误，只是有的程序会软渲染凑合运行，有的会直接崩溃或掉帧。  
- 要彻底解决，还是得把 libGL/libEGL 指向 Mali 版本、确保 rockchip_dri.so 被 GLVND 正确加载，再 `ldconfig`、重启验证；否则 Qt 只能靠软件路径，性能和 NPU 后续整合都会受限。

**建议优先级**

- **先切换到自管 GStreamer 管线**（立即执行）  
  - 你的摄像头功能和后续 NPU 推理都需要掌握原始视频流，GStreamer+appsink 是最佳形态；这一步不依赖 GPU，能先稳定工作，也便于后面接 RKNN。  
  - Qt 里只负责嵌入/显示和信号槽逻辑，避免 OpenGL 依赖，现状马上可用。

- **在此基础上，继续修复 Mali/OpenGL 驱动**（中期推进）  
  - 即便已经换成 GStreamer，桌面和其他图形应用仍会因 llvmpipe 占 CPU。恢复 libmali 可以提升整体性能，也为将来需要 GL 的功能留路。  
  - 修复手段：更新厂商固件或手工把 libGL/EGL 软链接指向 libmali，`ldconfig` 后重启验证 `glxinfo -B` 变成 Mali-G52。

- **不建议回到 QtMultimedia 方案**  
  - 它受限于 OpenGL 渲染，即使修好驱动，也不利于后续的零拷贝、RKNN 集成，维护价值较低。

所以当前最佳路线是：立即用 GStreamer 管道+appsink 替换 QtMultimedia，管线中预留 RGA/RKNN 接口；然后并行推进 Mali 驱动修复，待 GPU 恢复后整个系统性能都会上一个台阶。