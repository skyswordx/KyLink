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