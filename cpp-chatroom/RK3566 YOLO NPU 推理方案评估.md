# **RK3566 平台 C++/Qt 项目集成 YOLO NPU 加速技术方案评估与实施路径**

## **第 1 部分：架构分析与最终建议：C++ vs. Python 用于 RK3566**

本部分评估所考虑的两种技术路径，并根据 RK3566 系统的资源限制和视频流需求，提供明确的架构建议。

### **1.1 评估方案一：Python 推理 (rknn-toolkit-lite2) 与 C++/Qt 集成**

该方法涉及让 C++/Qt 主应用程序与一个单独的 Python 进程通信，该进程使用 RKNN-Toolkit-Lite2 1 处理 NPU 推理。这种方法在 Rockchip 相关的示例中很常见 4，但对于生产环境中的 C++ 应用程序来说，存在严重的集成挑战。

#### **1.1.1 集成方法 A：进程间通信 (IPC)**

* **实施：** C++/Qt 应用程序将使用 QProcess 9 来启动和管理一个 Python 脚本子进程。视频帧（输入）和检测结果（输出）将通过标准 I/O（管道）、本地套接字 (Sockets) 11 或 Unix 域套接字 12 等 IPC 机制进行交换。
* **性能分析（关键瓶颈）：** 这是该方案的致命弱点，尤其是在处理视频流时。
  * **高昂的数据传输开销：** 视频流涉及以高频率（例如每秒 30 次）传输大量数据（每帧数百 KB 到数 MB）。
  * **IPC 延迟：** IPC 性能基准测试 12 表明，虽然共享内存可以实现微秒甚至纳秒级的延迟，但管道和套接字的速度要慢几个数量级（通常是毫秒级）。
  * **序列化开销：** 原始图像数据（例如来自 QImage 或 cv::Mat）必须在 C++ 进程中进行序列化，通过 IPC 层发送，然后在 Python 进程中反序列化（例如转换为 NumPy 数组）。在资源受限的 RK3566 14 上，这个过程会产生纯粹的 CPU 开销。
  * **C++ vs Python 延迟：** 比较 C++ 和 Python 进行视频采集的研究表明，C++ 具有显著更低的 CPU 使用率和延迟 16。引入 Python IPC 桥接器将重新引入 C++ 开发人员通常试图避免的所有性能开销 19。
* 相关资料参考：
  * ([https://stackoverflow.com/questions/15127047/qt-calling-external-python-script](https://stackoverflow.com/questions/15127047/qt-calling-external-python-script)) 9
  * ([https://medium.com/nerd-for-tech/developing-a-live-video-streaming-application-using-socket-programming-with-python-6bc24e522f19](https://medium.com/nerd-for-tech/developing-a-live-video-streaming-application-using-socket-programming-with-python-6bc24e522f19)) 11
  * ([https://github.com/brylee10/unix-ipc-benchmarks](https://github.com/brylee10/unix-ipc-benchmarks)) 12
  * ([https://stackoverflow.com/questions/2635272/fastest-low-latency-method-for-inter-process-communication-between-java-and-c](https://stackoverflow.com/questions/2635272/fastest-low-latency-method-for-inter-process-communication-between-java-and-c)) 13
  * ([https://forums.developer.nvidia.com/t/c-collects-video-much-faster-than-python-is-it-reasonable/315391](https://forums.developer.nvidia.com/t/c-collects-video-much-faster-than-python-is-it-reasonable/315391)) 16
  * ([https://stackoverflow.com/questions/16583569/qt-using-c-or-pyqt-which-is-mostly-preffered-for-todays-linux-based-embedded](https://stackoverflow.com/questions/16583569/qt-using-c-or-pyqt-which-is-mostly-preffered-for-todays-linux-based-embedded)) 19
  * ([https://stackoverflow.com/questions/13498445/calling-a-c-functions-through-a-python-script](https://stackoverflow.com/questions/13498445/calling-a-c-functions-through-a-python-script)) 20

#### **1.1.2 集成方法 B：直接嵌入 (Embedding)**

* **实施：** 一种更复杂的方法是使用 Python C-API 21 或 pybind11 22 等工具，将 Python 解释器直接嵌入到 C++/Qt 应用程序中。
* **性能分析：** 虽然这消除了 IPC 瓶颈，但它引入了两个新问题：
  * 数据编组 (Marshalling) 开销： 每一帧仍然需要从 C++ 对象 (cv::Mat 或 QImage) “编组”为 Python 对象（PyObject\*，通常是 NumPy 数组）。当这个“垫片层”(shim layer) 26 被高频调用时，转换开销本身就是一个显著的性能瓶颈。
  * **运行时开销：** 这要求在嵌入式应用程序中打包并初始化完整的 Python 运行时环境，这会增加内存占用和应用程序的整体复杂性，不适合资源严格受限的系统 19。
* 相关资料参考：
  * ([https://stackoverflow.com/questions/13498445/calling-a-c-functions-through-a-python-script](https://stackoverflow.com/questions/13498445/calling-a-c-functions-through-a-python-script)) 21
  * ([https://github.com/pybind/pybind11/issues/2760](https://github.com/pybind/pybind11/issues/2760)) 22
  * ([https://www.reddit.com/r/cpp/comments/hlabb3/what\_is\_the\_best\_way\_to\_embed\_python\_on\_a\_qtc/](https://www.reddit.com/r/cpp/comments/hlabb3/what_is_the_best_way_to_embed_python_on_a_qtc/)) 23
  * ([https://forum.qt.io/topic/159925/clang-tidy-fails-to-analyze-qt-project-that-uses-pybind11-due-to-reserved-slots-keyword](https://forum.qt.io/topic/159925/clang-tidy-fails-to-analyze-qt-project-that-uses-pybind11-due-to-reserved-slots-keyword)) 24
  * ([https://zpz.github.io/blog/speeding-up-python-with-cpp-and-pybind11/](https://zpz.github.io/blog/speeding-up-python-with-cpp-and-pybind11/)) 25
  * ([https://stackoverflow.com/questions/76812995/why-c-is-slower-than-python-to-evaluate-functions](https://stackoverflow.com/questions/76812995/why-c-is-slower-than-python-to-evaluate-functions)) 26
  * ([https://www.reddit.com/r/programming/comments/17507jf/how\_to\_avoid\_pybind11\_and\_write\_5x\_faster\_cpython/](https://www.reddit.com/r/programming/comments/17507jf/how_to_avoid_pybind11_and_write_5x_faster_cpython/)) 27
  * ([https://stackoverflow.com/questions/16583569/qt-using-c-or-pyqt-which-is-mostly-preffered-for-todays-linux-based-embedded](https://stackoverflow.com/questions/16583569/qt-using-c-or-pyqt-which-is-mostly-preffered-for-todays-linux-based-embedded)) 19

### **1.2 评估方案二：原生 C++ 推理 (librknnrt)**

* **实施：** 此方法涉及将 Rockchip 的 C/C++ 运行时库 librknnrt.so 1 直接链接到现有的 C++/Qt 项目中。
* **性能分析：**
  * **零 IPC 开销：** 所有数据都保留在应用程序的 *单一进程内存空间* 内。
  * **直接内存访问：** 可以获取指向 QImage::bits() 31 或 cv::Mat::data 32 的原始图像数据指针 (void\*)，并将其直接传递给 NPU API rknn\_inputs\_set 34。
  * **“零拷贝” (Zero-Copy) 潜力：** 通过将 librknnrt 与 librga（Rockchip 2D 图形加速器）36 相结合，可以创建一个流水线，其中图像被解码、预处理（缩放、颜色空间转换）并提交给 NPU，而无需 CPU 进行任何中间内存拷贝。RKNN API 明确包含为 RGA 集成设计的内存函数 30，从而实现 RK3588 仓库 36 中提到的“零拷贝 API”。
* 相关资料参考：
  * ([https://github.com/rockchip-linux/rknn-toolkit](https://github.com/rockchip-linux/rknn-toolkit)) 1
  * ([https://github.com/rockchip-linux/rknn-toolkit2](https://github.com/rockchip-linux/rknn-toolkit2)) 2
  * ([https://github.com/airockchip/rknpu](https://github.com/airockchip/rknpu)) 28
  * ((https://github.com/Qengineering/YoloV5-NPU)) 29
  * ([https://forum.qt.io/topic/80810/passing-qimage-data-from-a-text-file-or-data-stream](https://forum.qt.io/topic/80810/passing-qimage-data-from-a-text-file-or-data-stream)) 31
  * ([https://docs.opencv.org/3.4/d3/d63/classcv\_1\_1Mat.html](https://docs.opencv.org/3.4/d3/d63/classcv_1_1Mat.html)) 32
  * ([https://stackoverflow.com/questions/61042167/how-to-convert-cvmat-to-void](https://stackoverflow.com/questions/61042167/how-to-convert-cvmat-to-void)) 33
  * ((https://repo.rock-chips.com/rk1808/rknn-api/Rockchip\_User\_Guide\_RKNN\_API\_V1.4.0\_EN.pdf)) 34
  * ([https://www.easy-eai.com/document\_details/18/614](https://www.easy-eai.com/document_details/18/614)) 35
  * ([https://github.com/yuunnn-w/rknn-cpp-yolo](https://github.com/yuunnn-w/rknn-cpp-yolo)) 36
  * ([https://github.com/airockchip/rknn\_model\_zoo](https://github.com/airockchip/rknn_model_zoo)) 37
  * ([https://github.com/airockchip/librga](https://github.com/airockchip/librga)) 38

### **1.3 结论与最终建议**

大量 Python 示例的存在 5 是一个 *误导性指标*。这些示例是为快速原型设计和简单的独立脚本而创建的，*并非*为集成到现有的、高性能的 C++/Qt 视频处理应用程序中而设计。

对于一个涉及在资源受限的 RK3566 14 上处理实时视频流的项目，与 Python 集成相关的 IPC 或嵌入开销是一个架构上的 *缺陷*，它将导致不可接受的延迟和高 CPU 负载 16。

**最终建议：** 必须选择 **方案二：原生 C++ 推理**。这是在嵌入式环境中为视频流应用实现高性能、低延迟和低资源消耗的唯一健壮途径。本报告的其余部分将专注于实施此解决方案的最佳计划。

**表 1：架构对比：Python 集成 vs. 原生 C++ 集成**

| 解决方案                 | 实现方法                       | 延迟 (视频流) | CPU 开销 (数据传输)           | 额外内存占用          | C++/Qt 集成复杂度            |
| :----------------------- | :----------------------------- | :------------ | :---------------------------- | :-------------------- | :--------------------------- |
| **方案一：Python** | IPC (QProcess\+ Sockets/Pipes) | **高**  | **高** (序列化 \+ IPC)  | 中 (需要 Python 进程) | **高** (进程管理, IPC) |
| **方案一：Python** | 嵌入 (pybind11 / C-API)        | 中            | 中 (数据编组) 26              | 高 (Python 运行时)    | 高 (构建系统, GIL)           |
| **方案二：C++**    | 原生链接 (librknnrt.so)        | **低**  | **无** (进程内指针传递) | 低 (仅 librknnrt 库)  | 中 (C-API, 线程)             |

## **第 2 部分：可行性分析：RKNN C-API 从 RK3588 到 RK3566 的可移植性**

本部分直接解决关于 \[yuunnn-w/rknn-cpp-yolo\](https://github.com/yuunnn-w/rknn-cpp-yolo) 仓库 36 针对 RK3588 而非 RK3566 的核心担忧。

### **2.1 RKNPU2 SDK 生态系统（关键背景）**

RK3566 15 和 RK3588 41 在原始性能上差异巨大，但它们是 *同一个 RKNPU2 SDK 家族* 的组成部分。

* 相关资料参考：
  * ([https://linuxgizmos.com/tiny-mini-pc-shows-off-rockchip-rk3566/](https://linuxgizmos.com/tiny-mini-pc-shows-off-rockchip-rk3566/)) 15
  * ([https://www.96rocks.com/blog/2020/10/21/rockchip-rk3566-highlights/?ref=rgb](https://www.96rocks.com/blog/2020/10/21/rockchip-rk3566-highlights/?ref=rgb)) 39
  * ((([https://www.boardcon.com/download/Rockchip\_RK3566\_Datasheet\_V1.1.pdf](https://www.boardcon.com/download/Rockchip_RK3566_Datasheet_V1.1.pdf)))) 40

Rockchip 官方文档 1 一贯地将 RK3566 和 RK3588 *共同* 列为 rknn-toolkit2（PC 端工具）和 rknnpu2（设备端驱动和运行时）的支持平台。

这意味着它们共享一个通用的 API 抽象层。C++ 级别的主要交互是通过 rknn\_api.h 头文件 29 和 librknnrt.so 运行时库 30 进行的，它们在这些平台之间是兼容的。

### **2.2 C++ 参考项目 (\[yuunnn-w/rknn-cpp-yolo\](https://github.com/yuunnn-w/rknn-cpp-yolo)) 分析**

分析这个 RK3588 项目 36 可以揭示哪些部分是可移植的，哪些是特定于平台的。

#### **2.2.1 可移植的组件（应重用的部分）**

* **核心 API 调用：** 整个推理流程（rknn\_init, rknn\_query, rknn\_inputs\_set, rknn\_run, rknn\_outputs\_get, rknn\_destroy）34 在 API 层面是完全相同的。
* **RGA 硬件预处理：** 该项目提到了“RGA 硬件加速预处理”36。RK3566 *同样* 拥有 RGA (Rockchip 2D Accelerator) 28。使用 librga C-API 进行硬件缩放和颜色空间转换的概念是 1:1 可重用的。
* **零拷贝 (Zero-Copy) API：** “零拷贝” 36 并非 RK3588 独有，而是 RKNPU2 API 的一个特性，它允许 RGA 缓冲区被 NPU 直接用作输入，无需 CPU 拷贝 30。这对于优化 RK3566 的性能至关重要。

#### **2.2.2 不可移植的组件（应忽略的部分）**

* **多核推理：** 该项目提到了“三个 NPU 核心的并发推理”36。
* 这是 *特定于 RK3588 硬件* 的优化，该芯片拥有三个 NPU 核心 49。RK3566 只有一个 NPU 核心 14。
* RKNPU2 SDK 包含用于管理多核 NPU 的 API 调用 50，但这些在 RK3566 上无效。在 RK3566 上调用 rknn\_run 将自动使用其唯一的 NPU 核心。因此，应忽略或移除任何与核心亲和性或多核调度相关的代码。

#### **2.2.3 模型文件的兼容性**

* 一个为 RK3588 编译的 .rknn 模型文件 *不能* 在 RK3566 上运行。
* 模型转换过程（见第 3 部分）必须在 PC 端的 rknn-toolkit2 中明确指定 target\_platform='rk3566' 5。这将确保工具包生成针对 RK3566 的 1.0 TOPS NPU 14 优化的指令。
* 相关资料参考：
  * ([https://github.com/yuunnn-w/rknn-cpp-yolo](https://github.com/yuunnn-w/rknn-cpp-yolo)) 36
  * ((https://repo.rock-chips.com/rk1808/rknn-api/Rockchip\_User\_Guide\_RKNN\_API\_V1.4.0\_EN.pdf)) 34
  * ([https://github.com/airockchip/rknpu](https://github.com/airockchip/rknpu)) 28
  * ([https://github.com/airockchip/rknn\_model\_zoo](https://github.com/airockchip/rknn_model_zoo)) 37
  * ([https://github.com/airockchip/librga](https://github.com/airockchip/librga)) 38
  * ((https://www.boardcon.com/download/Rockchip\_RK3566\_Datasheet\_V1.1.pdf)) 40
  * ([https://github.com/rockchip-linux/mpp](https://github.com/rockchip-linux/mpp)) 48
  * ([https://github.com/rockchip-linux/rknn-toolkit](https://github.com/rockchip-linux/rknn-toolkit)) 30
  * ((https://www.reddit.com/r/RockchipNPU/comments/1la7xdr/rk3566\_rk3576\_and\_rk3588\_compared/)) 49
  * ([https://github.com/rockchip-linux/rknpu2](https://github.com/rockchip-linux/rknpu2)) 50
  * ([https://doc.embedfire.com/linux/rk356x/Python/en/latest/ai/yolov5.html](https://doc.embedfire.com/linux/rk356x/Python/en/latest/ai/yolov5.html)) 5
  * ([https://docs.radxa.com/en/rock5/rock5b/app-development/rknn\_toolkit\_lite2\_yolov5](https://docs.radxa.com/en/rock5/rock5b/app-development/rknn_toolkit_lite2_yolov5)) 8
  * ([https://avionchip.com/rockchip-rknn-model-zoo-tutorial/](https://avionchip.com/rockchip-rknn-model-zoo-tutorial/)) 52

### **2.3 结论：C++ 方案完全可行**

使用 C++ 进行推理不仅是可行的，而且是 Rockchip 平台预期的部署方式。所找到的 C++ 仓库（\[yuunnn-w/rknn-cpp-yolo\](https://github.com/yuunnn-w/rknn-cpp-yolo)）是一个极好的 *架构参考*（C-API \+ RGA），即使它是为不同的芯片构建的。将要编写的 C++ 推理代码 95% 都是相同的。

此外，系统信息（NPU 驱动 v0.9.8，内核 5.10.198）证实了该系统是现代且兼容的。该 v0.9.8 驱动与最新的 RKNPU2 SDK 版本相关联 53，并与 5.10 系列内核 54 兼容。

* 相关资料参考：
  * ([https://github.com/blakeblackshear/frigate/discussions/18878](https://github.com/blakeblackshear/frigate/discussions/18878)) 53
  * ((https://github.com/MichaIng/DietPi/issues/7254)) 54
  * ([https://www.reddit.com/r/OrangePI/comments/1lqdh32/rkmpp\_rk3566\_immich\_gpu\_acceleration/](https://www.reddit.com/r/OrangePI/comments/1lqdh32/rkmpp_rk3566_immich_gpu_acceleration/)) 55
  * ([https://github.com/armbian/linux-rockchip/issues/266](https://github.com/armbian/linux-rockchip/issues/266)) 57
  * (([https://github.com/Joshua-Riek/ubuntu-rockchip/issues/1093](https://github.com/Joshua-Riek/ubuntu-rockchip/issues/1093))) 58
  * ((https://www.reddit.com/r/RockchipNPU/comments/1gil8if/armbian\_builds\_with\_npu\_driver\_098/)) 59
  * ([https://docs.radxa.com/en/rock5/rock5b/app-development/rkllm\_install](https://docs.radxa.com/en/rock5/rock5b/app-development/rkllm_install)) 60
  * (([https://www.scribd.com/document/927338308/Rockchip-Developer-Guide-Linux-Software-En](https://www.scribd.com/document/927338308/Rockchip-Developer-Guide-Linux-Software-En))) 61

**表 2：RKNN C-API 兼容性分析：RK3588 vs. RK3566**

| 特性                    | RK3588 (参考平台)     | RK3566 (目标平台)     | 状态与所需操作                |
| :---------------------- | :-------------------- | :-------------------- | :---------------------------- |
| **硬件**          |                       |                       |                               |
| NPU 算力                | 6.0 TOPS 41           | 0.8-1.0 TOPS 14       | 硬件差异 (性能更低)           |
| NPU 核心数              | 3 核心 49             | 1 核心 14             | 硬件差异 (需忽略多核代码)     |
| 2D 加速器               | RGA 48                | RGA 28                | **硬件通用 (API 兼容)** |
| **软件 (RKNPU2)** |                       |                       |                               |
| 设备端 SDK              | RKNPU2                | RKNPU2 1              | **通用 (完全兼容)**     |
| C-API 头文件            | rknn\_api.h 29        | rknn\_api.h 29        | **通用 (完全兼容)**     |
| C-API 库                | librknnrt.so 30       | librknnrt.so 29       | **通用 (完全兼容)**     |
| NPU 驱动                | v0.9.8 (或更高)       | v0.9.8 (已安装)       | **通用 (完全兼容)**     |
| **模型文件**      |                       |                       |                               |
| 模型格式                | .rknn (target=rk3588) | .rknn (target=rk3566) | **必须重新转换**        |

## **第 3 部分：最佳实践方案：在 RK3566 上实施原生 C++ YOLO 推理**

这是一个详细的技术指南，用于在 RK3566 上实施推荐的 C++ 解决方案。

### **3.1 步骤一：在 RK3566 上配置 SDK 组件**

rknn-toolkit2 是一个 PC 端工具 1，不应安装在 RK3566 设备上。设备上需要的是 C-API "RKNN Runtime"。

1. **在 PC 上：** 克隆 rknn-toolkit2 或 rknn\_model\_zoo 63 仓库。
2. **定位文件：** 导航到 rknnpu2/runtime/Linux/librknn\_api/ 目录 29。
3. **复制库文件：** 将 aarch64/librknnrt.so 复制到 RK3566 上的标准库路径（例如 /usr/local/lib/）。
4. **复制头文件：** 将 include/ 目录下的 rknn\_api.h, rknn\_custom\_op.h 和 rknn\_matmul\_api.h 29 复制到 RK3566 上的标准包含路径（例如 /usr/local/include/）。
5. **验证：** 系统已安装 v0.9.8 NPU 驱动 53，librknnrt.so 将能够与该驱动正常通信。

* 相关资料参考：
  * ([https://github.com/rockchip-linux/rknn-toolkit](https://github.com/rockchip-linux/rknn-toolkit)) 1
  * ([https://github.com/rockchip-linux/rknn-toolkit2](https://github.com/rockchip-linux/rknn-toolkit2)) 2
  * ([https://doc.embedfire.com/linux/rk356x/Python/en/latest/ai/yolov5.html](https://doc.embedfire.com/linux/rk356x/Python/en/latest/ai/yolov5.html)) 5
  * (([https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage\_npu.html](https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage_npu.html))) 14
  * ((https://github.com/Qengineering/YoloV5-NPU)) 29
  * ([https://github.com/airockchip/rknn\_model\_zoo](https://github.com/airockchip/rknn_model_zoo)) 63

### **3.2 步骤二：模型转换（在 x86 PC 上）**

1. **环境：** 在 Ubuntu 18.04/20.04/22.04 x86 PC 上安装 rknn-toolkit2 (版本 1.6.0 或更高) 2。
2. **源模型：** 获取 .pt 或 .onnx 格式的 YOLO 模型（例如 YOLOv5s）5。rknn\_model\_zoo 52 提供了下载这些模型的脚本 5。
3. **转换：** 使用 rknn-toolkit2 的 Python API 进行转换（例如，rknn\_model\_zoo 中的 convert.py 脚本 5）。
4. **关键步骤：** 在转换配置中，*必须* 指定目标平台：Pythonrknn.config(target\_platform='rk3566')

   5。
5. **输出：** 生成 model.rknn 文件。将此文件复制到 RK3566 设备上。

* 相关资料参考：
  * ([https://github.com/rockchip-linux/rknn-toolkit2](https://github.com/rockchip-linux/rknn-toolkit2)) 2
  * ([https://doc.embedfire.com/linux/rk356x/Python/en/latest/ai/yolov5.html](https://doc.embedfire.com/linux/rk356x/Python/en/latest/ai/yolov5.html)) 5
  * ([https://docs.radxa.com/en/rock5/rock5b/app-development/rknn\_toolkit\_lite2\_yolov5](https://docs.radxa.com/en/rock5/rock5b/app-development/rknn_toolkit_lite2_yolov5)) 8
  * ([https://github.com/rockchip-linux/rknn-toolkit](https://github.com/rockchip-linux/rknn-toolkit)) 30
  * ([https://avionchip.com/rockchip-rknn-model-zoo-tutorial/](https://avionchip.com/rockchip-rknn-model-zoo-tutorial/)) 52
  * ((https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage\_npu.html)) 62
  * ([https://github.com/airockchip/rknn\_model\_zoo](https://github.com/airockchip/rknn_model_zoo)) 63
  * ([https://github.com/ultralytics/yolov5](https://github.com/ultralytics/yolov5)) 64
  * ((https://wiki.luckfox.com/Luckfox-Pico-Plus-Mini/RKNN)) 65
  * ([https://blog.csdn.net/Agastopgia/article/details/140325447](https://blog.csdn.net/Agastopgia/article/details/140325447)) 66
  * ([docs.ultralytics.com/integrations/rockchip-rknn/](https://docs.ultralytics.com/integrations/rockchip-rknn/)) 67

### **3.3 步骤三：设备端的高性能预处理 (C++)**

* **问题：** .rknn 模型（例如 YOLOv5s）期望特定格式的输入，例如 \`\`、UINT8、NHWC 格式 14。而视频源（摄像头或文件）提供的是不同格式（例如 1920x1080 BGR 或 YUV）。
* **CPU 瓶颈（错误方案）：** 使用 OpenCV 的 cv::resize 和 cv::cvtColor。虽然是 C++，但这将在 ARM Cortex-A55 CPU 15 上运行，效率极低，并将成为主要的性能瓶颈，使 NPU 的速度优势荡然无存 20。
* **硬件加速（正确方案）：** 使用 Rockchip 的 2D 加速器 RGA。RK3566 拥有此硬件 37。
  1. 在 Qt 项目中链接 librga.so（见第 4 部分）。此库由 Rockchip SDK 提供 28。
  2. 使用 RGA 的 C-API（通常在 rga\_api.h 中定义）来执行硬件加速的 *缩放*（例如 1920x1080 \-\> 640x640）和 *颜色空间转换*（例如 BGR \-\> RGB）。
  3. RGA 操作的输出是一个准备好送入 NPU 的图像缓冲区。
* 相关资料参考：
  * (([https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage\_npu.html](https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage_npu.html))) 14
  * ([https://linuxgizmos.com/tiny-mini-pc-shows-off-rockchip-rk3566/](https://linuxgizmos.com/tiny-mini-pc-shows-off-rockchip-rk3566/)) 15
  * ([https://stackoverflow.com/questions/13498445/calling-a-c-functions-through-a-python-script](https://stackoverflow.com/questions/13498445/calling-a-c-functions-through-a-python-script)) 20
  * ([https://github.com/airockchip/rknpu](https://github.com/airockchip/rknpu)) 28
  * ([https://github.com/airockchip/rknn\_model\_zoo](https://github.com/airockchip/rknn_model_zoo)) 37
  * ((https://www.boardcon.com/download/Rockchip\_RK3566\_Datasheet\_V1.1.pdf)) 40
  * ([https://portworld-solu.com/rk3588-vs-rk3568-vs-rk3566-whats-the-difference/](https://portworld-solu.com/rk3588-vs-rk3568-vs-rk3566-whats-the-difference/)) 41
  * ([https://github.com/rockchip-linux/mpp](https://github.com/rockchip-linux/mpp)) 48
  * ([https://github.com/ultralytics/yolov5](https://github.com/ultralytics/yolov5)) 64
  * ([https://www.armdesigner.com/article.php?id=278](https://www.armdesigner.com/article.php?id=278)) 68
  * ([https://stackoverflow.com/questions/15127047/qt-calling-external-python-script](https://stackoverflow.com/questions/15127047/qt-calling-external-python-script)) 69

### **3.4 步骤四：核心推理流水线 (C++)**

这是推理工作类（见第 4 部分）的核心。以下是必须实现的 C-API 工作流程，主要基于 34：

C++

\#**include** "rknn\_api.h"
//... 其他包含...

rknn\_context ctx;
// 1\. 初始化
// 从文件加载 model.rknn 到 model\_data 缓冲区
ret \= rknn\_init(\&ctx, model\_data, model\_size, 0, NULL);

// 2\. 查询模型信息（推荐）
rknn\_input\_output\_num io\_num;
ret \= rknn\_query(ctx, RKNN\_QUERY\_IN\_OUT\_NUM, \&io\_num, sizeof(io\_num));
// 查询输入/输出张量的属性
rknn\_tensor\_attr input\_attr;
input\_attr.index \= 0;
ret \= rknn\_query(ctx, RKNN\_QUERY\_INPUT\_TENSOR\_ATTR, \&input\_attr, sizeof(input\_attr));

//... (循环处理视频帧)...

// 3\. 设置输入 (每帧)
rknn\_input inputs;
memset(inputs, 0, sizeof(inputs));
inputs.index \= 0;
inputs.type \= RKNN\_TENSOR\_UINT8;    // 必须与模型匹配
inputs.fmt \= RKNN\_TENSOR\_NHWC;     // 必须与模型匹配 \[14, 34\]
inputs.buf \= (void\*)rga\_output\_buffer; // \*\*\* 关键：指向 RGA 处理后的数据 \*\*\*
inputs.size \= input\_attr.size\_with\_stride;
inputs.pass\_through \= 1; // 1 表示数据已预处理, 0 表示需要驱动内部转换
ret \= rknn\_inputs\_set(ctx, 1, inputs);

// 4\. 执行推理 (每帧)
ret \= rknn\_run(ctx, NULL); // 这是一个阻塞调用

// 5\. 获取输出 (每帧)
rknn\_output outputs; // YOLOv5/v8 通常有 3 个输出头
memset(outputs, 0, sizeof(outputs));
outputs.want\_float \= 0; // 获取原始 INT8 数据进行高效后处理
outputs.want\_float \= 0;
outputs.want\_float \= 0;
ret \= rknn\_outputs\_get(ctx, 3, outputs, NULL); // 阻塞直到推理完成

// 6\. C++ 后处理 (见 3.5)
// post\_process(outputs, \&input\_attr,...);

// 7\. 释放输出 (每帧)
ret \= rknn\_outputs\_release(ctx, 3, outputs);

//... (循环结束)...

// 8\. 销毁上下文 (程序退出时)ret \= rknn\_destroy(ctx);

* 相关资料参考：
  * (([https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage\_npu.html](https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage_npu.html))) 14
  * ((https://repo.rock-chips.com/rk1808/rknn-api/Rockchip\_User\_Guide\_RKNN\_API\_V1.4.0\_EN.pdf)) 34
  * ([https://www.easy-eai.com/document\_details/18/614](https://www.easy-eai.com/document_details/18/614)) 35

### **3.5 步骤五：后处理 (C++)**

rknn\_outputs\_get 不会返回边界框。它返回原始的 INT8 量化张量 14。必须编写 C++ 代码来执行：

1. **反量化：** 使用 rknn\_query 获取的 scale 和 zero\_point (zp) 14，将 INT8 输出转换为 float。
2. **解码：** 应用锚点框 (anchors) 和网格 (grid) 逻辑，将张量解码为实际的边界框坐标 (x, y, w, h)、置信度 (confidence) 和类别分数 (class scores)。
3. **非极大值抑制 (NMS)：** 过滤掉重叠的边界框。

这是一个复杂的过程。**强烈建议**不要从头开始编写，而是直接参考或复用 rknn\_model\_zoo 63 或 Qengineering 29 示例中的 C++ 后处理代码。

* 相关资料参考：
  * (([https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage\_npu.html](https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage_npu.html))) 14
  * ((https://github.com/Qengineering/YoloV5-NPU)) 29
  * ([https://github.com/airockchip/rknn\_model\_zoo](https://github.com/airockchip/rknn_model_zoo)) 63

### **3.6 步骤六：推荐的 RK3566 C++ 参考项目**

应优先使用以下专为 RK3566 优化的 C++ 示例，而不是 RK3588 仓库：

1. **Rockchip rknn\_model\_zoo (官方):**
   * **来源：** \[https://github.com/airockchip/rknn\_model\_zoo\](https://github.com/airockchip/rknn\_model\_zoo) 52
   * **内容：** 包含 YOLOv5 37, YOLOv8 52 等模型的 C-API 示例。
   * **操作：** 查找 examples/yolov5/cpp/。相关教程 52 提供了 RK3566 C++ demo 的构建和运行命令（例如 ./build-linux.sh \-t rk3566）。这是最权威的参考。
2. **Qengineering YoloV5-NPU (第三方):**
   * [ ] **来源：** (https://github.com/Qengineering/YoloV5-NPU) 29
   * [ ] **内容：** 一个专为 RK3566/68/88 编写的纯 C++ YOLOv5 项目。它提供了清晰的 librknnrt.so 链接方法 29 和性能基准（例如 YOLOv5s 在 RK3566 上为 11.7 FPS 29）。

* 相关资料参考：
  * ((https://github.com/Qengineering/YoloV5-NPU)) 29
  * ([https://github.com/airockchip/rknn\_model\_zoo](https://github.com/airockchip/rknn_model_zoo)) 37
  * ([https://avionchip.com/rockchip-rknn-model-zoo-tutorial/](https://avionchip.com/rockchip-rknn-model-zoo-tutorial/)) 52

**表 3：librknnrt C-API 核心功能**

| 函数                   | 描述 (基于 )                                                        |
| :--------------------- | :------------------------------------------------------------------ |
| rknn\_init             | 从内存缓冲区（.rknn 文件内容）加载模型并初始化上下文 (context)。    |
| rknn\_destroy          | 释放上下文及所有相关资源。                                          |
| rknn\_query            | 查询模型信息，如输入/输出数量、张量属性（维度、格式、量化参数）14。 |
| rknn\_inputs\_set      | 设置输入张量的数据缓冲区。这是将 RGA 输出传递给 NPU 的地方。        |
| rknn\_run              | 触发一次（异步）推理。                                              |
| rknn\_outputs\_get     | 阻塞等待，直到推理完成，并获取输出张量的句柄。                      |
| rknn\_outputs\_release | 释放 rknn\_outputs\_get 获取的输出张量。                            |
| rknn\_create\_mem      | (高级) 分配 NPU 可访问的内存，可与 RGA 结合实现零拷贝 30。          |

## **第 4 部分：将推理流水线集成到 Qt 5.12.8 项目中**

此部分专注于将第 3 部分构建的 C++ 流水线安全地集成到现有的、基于 IPMSG/Feiqiu 和 Qt 5.12.8 的 C++ 应用程序中，确保 GUI 保持响应。

### **4.1 步骤一：项目配置 (.pro 文件)**

使用 Qt 5.12，项目很可能使用 qmake (.pro 文件)。需要更新此文件以链接 RKNN 和 RGA 库。

**编辑 .pro 文件并添加：**

代码段

\# 添加 Rockchip API 头文件路径
INCLUDEPATH \+= /usr/local/include

\# 添加 Rockchip 库路径和库链接
\# \-L 指定库路径, \-l (小写L) 指定库名称
\# librknnrt.so \-\> \-lrknnrt
\# librga.so \-\> \-lrga
LIBS \+= \-L/usr/local/lib \-lrknnrt \-lrga

这与 72 中描述的 Qt 库链接方法一致。保存文件后，重新运行 qmake。

* 相关资料参考：
  * ([https://stackoverflow.com/questions/7182229/linking-to-shared-library-in-qt](https://stackoverflow.com/questions/7182229/linking-to-shared-library-in-qt)) 72
  * ([https://hwzen.myds.me:17001/qtdoc/qtdoc/sharedlibrary.html](https://hwzen.myds.me:17001/qtdoc/qtdoc/sharedlibrary.html)) 73

### **4.2 步骤二：异步、响应式 GUI 的架构（线程）**

* **问题：** rknn\_run 和 rknn\_outputs\_get 34 是 *阻塞* 调用。整个推理流水线（RGA \+ NPU \+ 后处理）将耗时（例如，\~85ms，基于 11.7 FPS 29）。如果在 Qt GUI 主线程中调用，*整个应用程序将冻结*，直到推理完成，导致应用无响应。
* **错误方案：** QProcess。如第 1 部分所述，这是用于运行 *外部* 程序的，会带来巨大的 IPC 开销 74。
* **正确的 Qt 5 方案：QThread \+ Worker-Object 模式** 76。这是在 Qt 中处理长时间运行的阻塞任务的标准方法。
* 相关资料参考：
  * (([https://github.com/Qengineering/YoloV5-NPU](https://github.com/Qengineering/YoloV5-NPU))) 29
  * (([https://repo.rock-chips.com/rk1808/rknn-api/Rockchip\_User\_Guide\_RKNN\_API\_V1.4.0\_EN.pdf](https://repo.rock-chips.com/rk1808/rknn-api/Rockchip_User_Guide_RKNN_API_V1.4.0_EN.pdf))) 34
  * ((https://www.qtcentre.org/threads/70061-Execute-a-QProcess-instead-of-a-QThread)) 74
  * ([https://stackoverflow.com/questions/43766559/is-this-an-appropriate-time-to-use-qthread-with-qprocess](https://stackoverflow.com/questions/43766559/is-this-an-appropriate-time-to-use-qthread-with-qprocess)) 75
  * ([https://stackoverflow.com/questions/66121444/using-qt-process-or-threads-to-run-functions](https://stackoverflow.com/questions/66121444/using-qt-process-or-threads-to-run-functions)) 76
  * ([https://www.kdab.com/the-eight-rules-of-multithreaded-qt/](https://www.kdab.com/the-eight-rules-of-multithreaded-qt/)) 78

**实施架构：**

1. **创建 Worker 类 (C++):**C++// RKNNInferenceWorker.h\#**include** \<QObject\>\#**include** \<QImage\>\#**include** \<QVariantList\> // 用于传递检测结果\#**include** "rknn\_api.h"

   class RKNNInferenceWorker : public QObject {Q\_OBJECTpublic:explicit RKNNInferenceWorker(QObject \*parent \= nullptr);\~RKNNInferenceWorker();

   public slots:// 此槽在 QThread::started 信号发出时调用void initialize();

   // 此槽用于从 GUI 线程接收新帧void processFrame(const QImage \&frame);

   signals:// 此信号将检测结果（例如 QList\<QRectF\>）发送回 GUI 线程void resultsReady(const QVariantList \&detections);

   private:rknn\_context m\_rknnContext;//... rga\_context, input\_attrs 等成员变量

   // 内部函数，包含第 3.3 和 3.4 节的逻辑void runInferencePipeline(const QImage \&frame);};
2. **在主窗口中设置线程：**
   C++
   // MainWindow.cpp
   \#**include** \<QThread\>
   \#**include** "RKNNInferenceWorker.h"

   MainWindow::MainWindow(/\*...\*/) {
   //... 现有 UI 和 Feiqiu 逻辑...

   QThread\* rknnThread \= new QThread(this);
   RKNNInferenceWorker\* rknnWorker \= new RKNNInferenceWorker();

   // 1\. 将 worker 移动到新线程
   rknnWorker-\>moveToThread(rknnThread);

   // 2\. 连接信号和槽
   // 线程启动时，初始化 worker (rknn\_init)
   connect(rknnThread, \&QThread::started, rknnWorker, \&RKNNInferenceWorker::initialize);

   // 当 GUI 线程有新帧时，将其发送给 worker 处理
   // (假设 'this' 有一个 'newFrameAvailable' 信号)
   connect(this, \&MainWindow::newFrameAvailable,
   rknnWorker, \&RKNNInferenceWorker::processFrame);

   // 当 worker 完成处理时，在 GUI 线程中接收结果
   connect(rknnWorker, \&RKNNInferenceWorker::resultsReady,
   this, \&MainWindow::onResultsReady); // onResultsReady 是 GUI 线程中的槽

   // 3\. 启动线程
   rknnThread-\>start();
   }

此架构确保所有繁重的 RKNN/RGA 工作都在 rknnThread 上发生，而 GUI 主线程保持 100% 响应。

### **4.3 步骤三：Qt/C++ 中的高效图像数据处理**

* **问题：** 如何将 GUI 线程中的 QImage 或 cv::Mat 数据安全、高效地传递给 rknn\_inputs\_set 所需的 void\*？
* **数据流：**
  1. GUI 线程获取 QImage（例如来自 Qt 多媒体）或 cv::Mat（例如来自 OpenCV VideoCapture）。
  2. GUI 线程发出信号：emit newFrameAvailable(image);
  3. Worker 线程的槽 processFrame(const QImage \&frame) 接收数据。
* **指针提取（在 Worker 线程中）：**
  * **来自 QImage:** const uchar\* dataPtr \= frame.constBits(); 31。需要确保 frame.format() 是 RGA 期望的格式（例如 Format\_RGB888 或 Format\_BGR888）。
  * **来自 cv::Mat:** void\* dataPtr \= (void\*)mat.data; 32。
* **线程安全（关键）**
  * QImage 是*隐式共享*（Copy-on-Write）的。通过信号槽传递 QImage 通常是线程安全的 79。
  * cv::Mat 是一个*智能指针*。通过信号槽传递它，只是复制了指针，而不是数据 80。如果 GUI 线程在 Worker 线程仍在处理第 N 帧时就覆盖了第 N 帧的数据，将导致数据损坏或崩溃。
  * **解决方案：** 如 80 中所建议，如果使用 cv::Mat，Worker 槽接收到 cv::Mat 后**必须**做的第一件事就是调用 .clone()，以创建数据的*深度拷贝*，确保线程安全。cv::Mat frameCopy \= frame.clone();。
* 相关资料参考：
  * ([https://forum.qt.io/topic/80810/passing-qimage-data-from-a-text-file-or-data-stream](https://forum.qt.io/topic/80810/passing-qimage-data-from-a-text-file-or-data-stream)) 31
  * ([https://docs.opencv.org/3.4/d3/d63/classcv\_1\_1Mat.html](https://docs.opencv.org/3.4/d3/d63/classcv_1_1Mat.html)) 32
  * ([https://stackoverflow.com/questions/61042167/how-to-convert-cvmat-to-void](https://stackoverflow.com/questions/61042167/how-to-convert-cvmat-to-void)) 33
  * ([https://www.qtcentre.org/threads/67266-Pass-back-QImage-from-thread-repeatedly-deletes-proper-way](https://www.qtcentre.org/threads/67266-Pass-back-QImage-from-thread-repeatedly-deletes-proper-way)) 79
  * ([https://forum.qt.io/topic/89034/passing-mat-in-form-of-signal](https://forum.qt.io/topic/89034/passing-mat-in-form-of-signal)) 80
  * ([https://stackoverflow.com/questions/20691414/qt-qml-send-qimage-from-c-to-qml-and-display-the-qimage-on-gui](https://stackoverflow.com/questions/20691414/qt-qml-send-qimage-from-c-to-qml-and-display-the-qimage-on-gui)) 81

**表 4：Qt 中的异步集成模式**

| 比较项             | 错误方案 (QProcess)               | 正确方案 (QThread\+ Worker)     |
| :----------------- | :-------------------------------- | :------------------------------ |
| **目的**     | 运行一个*外部* 可执行文件 74    | 在*内部* 运行 C++ 函数 76     |
| **通信**     | IPC (管道/套接字)。*高* 延迟 12 | 信号/槽。*极低* 延迟 (进程内) |
| **数据传输** | *高* 开销 (视频帧序列化)        | *低* 开销 (指针/隐式共享)     |
| **集成**     | 两个独立进程。难以调试。          | 单一进程，多个线程。易于集成。  |
| **适用性**   | **不适用**                  | **理想**                  |

## **第 5 部分：总结与后续步骤**

### **5.1 核心建议摘要**

1. **拒绝 Python 路径：** 对于 C++/Qt 视频应用，Python 路径 5 是一个性能陷阱。IPC 和数据编组开销 12 将导致不可接受的延迟。
2. **采用 C++ 路径：** 这是在 RK3566 上实现高性能、低延迟的正确方案。RKNPU2 C-API (librknnrt.so) 在 RK3566 和 RK3588 之间是兼容的 1。
3. **使用正确参考：** 忽略 RK3588 仓库 36 的平台特定代码。*立即开始* 使用官方 rknn\_model\_zoo 52 或 Qengineering 的 YoloV5-NPU 29，它们包含针对 RK3566 的 C++ 示例 52。
4. **关注完整流水线：** 性能取决于三个硬件加速步骤：librga 预处理 37、librknnrt 推理 34 以及高效的 C++ 后处理。
5. **利用 Qt 线程：** 使用 QThread \+ Worker-Object 模式 76 来集成阻塞的 C-API，保持 GUI 的响应能力。

### **5.2 实施清单**

1. **\[PC\]** 安装 rknn-toolkit2 2。
2. **\[PC\]** 使用 target\_platform='rk3566' 5 将 YOLO ONNX 模型转换为 model-rk3566.rknn。
3. \*\*\*\* 将 librknnrt.so 和 librga.so 复制到 /usr/local/lib/。
4. \*\*\*\* 将 rknn\_api.h 和 rga\_api.h 复制到 /usr/local/include/。
5. \*\*\*\* 将 model-rk3566.rknn 复制到 Qt 应用程序的资源目录。
6. **\[Qt 项目\]** 编辑 .pro 文件，添加 LIBS \+= \-lrknnrt \-lrga 72。
7. **\[Qt 项目\]** 创建 RKNNInferenceWorker QObject 类。
8. **\[Qt 项目\]** 在 Worker 中实现 RGA 预处理和 rknn\_ API 核心逻辑（参考第 3.3 和 3.4 节）。
9. **\[Qt 项目\]** 从 rknn\_model\_zoo 63 或 Qengineering 29 借用 C++ 后处理（NMS、解码）代码。
10. **\[Qt 项目\]** 在 MainWindow 中，设置 QThread，moveToThread，并连接信号/槽（参考第 4.2 节）。

### **5.3 预期性能**

必须设定符合实际的性能预期。RK3566 (1.0 TOPS) 14 远不及 RK3588 (6.0 TOPS) 41。

Qengineering 的 C++ 基准测试 29 是最佳参考。在 RK3566 上，使用完整的 C++ 和 RGA 加速流水线，可以预期：

* **YOLOv5n:** \~19.5 FPS
* **YOLOv5s-relu:** \~14.8 FPS
* **YOLOv8n:** \~18.2 FPS
* **YOLOv8s:** \~8.9 FPS

这些性能指标对于许多实时检测任务是完全足够的，但前提是必须严格遵循本报告中详述的 C++ 和硬件加速路径。

* 相关资料参考：
  * ((https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage\_npu.html)) 14
  * ([https://linuxgizmos.com/tiny-mini-pc-shows-off-rockchip-rk3566/](https://linuxgizmos.com/tiny-mini-pc-shows-off-rockchip-rk3566/)) 15
  * ((https://github.com/Qengineering/YoloV5-NPU)) 29
  * ([https://portworld-solu.com/rk3588-vs-rk3568-vs-rk3566-whats-the-difference/](https://portworld-solu.com/rk3588-vs-rk3568-vs-rk3566-whats-the-difference/)) 41

#### **引用的著作**

1. RKNN Installation | Radxa Docs, 访问时间为 十一月 12, 2025， [https://docs.radxa.com/en/rock5/rock5b/app-development/rknn\_install](https://docs.radxa.com/en/rock5/rock5b/app-development/rknn_install)
2. rockchip-linux/rknn-toolkit2 \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/rockchip-linux/rknn-toolkit2](https://github.com/rockchip-linux/rknn-toolkit2)
3. r/RockchipNPU Wiki: Guide to Rockchip NPU Development \- Reddit, 访问时间为 十一月 12, 2025， [https://www.reddit.com/r/RockchipNPU/wiki/index/](https://www.reddit.com/r/RockchipNPU/wiki/index/)
4. 7\. YOLOv5 — \[Embedfire\]Practical Guide to Python Application Development — Based on LubanCat-RK Series Boards 文档, 访问时间为 十一月 12, 2025， [https://doc.embedfire.com/linux/rk356x/Python/en/latest/ai/yolov5.html](https://doc.embedfire.com/linux/rk356x/Python/en/latest/ai/yolov5.html)
5. Deploy YOLOv5 Object Detection on the Board \- Radxa Docs, 访问时间为 十一月 12, 2025， [https://docs.radxa.com/en/rock5/rock5b/app-development/rknn\_toolkit\_lite2\_yolov5](https://docs.radxa.com/en/rock5/rock5b/app-development/rknn_toolkit_lite2_yolov5)
6. Docs Integration Page \- Rockchip/RKNN · Issue \#13566 · ultralytics/ultralytics \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/ultralytics/ultralytics/issues/13566](https://github.com/ultralytics/ultralytics/issues/13566)
7. Deploy YOLOv8 Object Detection on the Board \- Radxa Docs, 访问时间为 十一月 12, 2025， [https://docs.radxa.com/en/rock5/rock5c/app-development/rknn\_toolkit\_lite2\_yolov8](https://docs.radxa.com/en/rock5/rock5c/app-development/rknn_toolkit_lite2_yolov8)
8. Rockchip RKNN Export for Ultralytics YOLO11 Models, 访问时间为 十一月 12, 2025， [https://docs.ultralytics.com/integrations/rockchip-rknn/](https://docs.ultralytics.com/integrations/rockchip-rknn/)
9. Qt Calling External Python Script \- c++ \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/15127047/qt-calling-external-python-script](https://stackoverflow.com/questions/15127047/qt-calling-external-python-script)
10. How to use/execute a Python script inside QtCreator C++ project \- Qt Forum, 访问时间为 十一月 12, 2025， [https://forum.qt.io/topic/84361/how-to-use-execute-a-python-script-inside-qtcreator-c-project](https://forum.qt.io/topic/84361/how-to-use-execute-a-python-script-inside-qtcreator-c-project)
11. Developing a Live Video Streaming Application using Socket Programming with Python | by Amima Shifa | Nerd For Tech | Medium, 访问时间为 十一月 12, 2025， [https://medium.com/nerd-for-tech/developing-a-live-video-streaming-application-using-socket-programming-with-python-6bc24e522f19](https://medium.com/nerd-for-tech/developing-a-live-video-streaming-application-using-socket-programming-with-python-6bc24e522f19)
12. Benchmarks for various IPC implementations on UNIX in C++ \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/brylee10/unix-ipc-benchmarks](https://github.com/brylee10/unix-ipc-benchmarks)
13. fastest (low latency) method for Inter Process Communication between Java and C/C++, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/2635272/fastest-low-latency-method-for-inter-process-communication-between-java-and-c](https://stackoverflow.com/questions/2635272/fastest-low-latency-method-for-inter-process-communication-between-java-and-c)
14. 3\. NPU — Firefly Wiki, 访问时间为 十一月 12, 2025， [https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage\_npu.html](https://wiki.t-firefly.com/en/ROC-RK3566-PC/usage_npu.html)
15. Tiny mini-PC shows off Rockchip RK3566 \- LinuxGizmos.com, 访问时间为 十一月 12, 2025， [https://linuxgizmos.com/tiny-mini-pc-shows-off-rockchip-rk3566/](https://linuxgizmos.com/tiny-mini-pc-shows-off-rockchip-rk3566/)
16. C++ collects video much faster than Python, is it reasonable? \- Jetson Xavier NX, 访问时间为 十一月 12, 2025， [https://forums.developer.nvidia.com/t/c-collects-video-much-faster-than-python-is-it-reasonable/315391](https://forums.developer.nvidia.com/t/c-collects-video-much-faster-than-python-is-it-reasonable/315391)
17. Measuring Video Latency Using OpenCV | by William Horn \- Medium, 访问时间为 十一月 12, 2025， [https://medium.com/@wbhorn/measuring-video-latency-using-opencv-c36c9fd14cc6](https://medium.com/@wbhorn/measuring-video-latency-using-opencv-c36c9fd14cc6)
18. Python or C++ for fast recording? \- Raspberry Pi Forums, 访问时间为 十一月 12, 2025， [https://forums.raspberrypi.com/viewtopic.php?t=167540](https://forums.raspberrypi.com/viewtopic.php?t=167540)
19. Qt using C++ or PyQT; which is mostly preffered for todays linux based embedded application development? \[closed\] \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/16583569/qt-using-c-or-pyqt-which-is-mostly-preffered-for-todays-linux-based-embedded](https://stackoverflow.com/questions/16583569/qt-using-c-or-pyqt-which-is-mostly-preffered-for-todays-linux-based-embedded)
20. Does performance differ between Python or C++ coding of OpenCV? \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/13432800/does-performance-differ-between-python-or-c-coding-of-opencv](https://stackoverflow.com/questions/13432800/does-performance-differ-between-python-or-c-coding-of-opencv)
21. Calling a C++ Functions through a Python Script \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/13498445/calling-a-c-functions-through-a-python-script](https://stackoverflow.com/questions/13498445/calling-a-c-functions-through-a-python-script)
22. What is the best way to embed python on a Qt/C++ applicattion \[closed\] : r/cpp \- Reddit, 访问时间为 十一月 12, 2025， [https://www.reddit.com/r/cpp/comments/hlabb3/what\_is\_the\_best\_way\_to\_embed\_python\_on\_a\_qtc/](https://www.reddit.com/r/cpp/comments/hlabb3/what_is_the_best_way_to_embed_python_on_a_qtc/)
23. \[FEAT\] Should include more comprehensive benchmarking (primarily performance) · Issue \#2760 · pybind/pybind11 \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/pybind/pybind11/issues/2760](https://github.com/pybind/pybind11/issues/2760)
24. Speeding up Python with C++ and pybind11 \- zpz –, 访问时间为 十一月 12, 2025， [https://zpz.github.io/blog/speeding-up-python-with-cpp-and-pybind11/](https://zpz.github.io/blog/speeding-up-python-with-cpp-and-pybind11/)
25. Clang-Tidy fails to analyze QT project that uses Pybind11 due to reserved 'slots' keyword, 访问时间为 十一月 12, 2025， [https://forum.qt.io/topic/159925/clang-tidy-fails-to-analyze-qt-project-that-uses-pybind11-due-to-reserved-slots-keyword](https://forum.qt.io/topic/159925/clang-tidy-fails-to-analyze-qt-project-that-uses-pybind11-due-to-reserved-slots-keyword)
26. Why C++ is slower than Python to evaluate functions? \[closed\] \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/76812995/why-c-is-slower-than-python-to-evaluate-functions](https://stackoverflow.com/questions/76812995/why-c-is-slower-than-python-to-evaluate-functions)
27. How to avoid PyBind11 and write 5x faster CPython bindings : r/programming \- Reddit, 访问时间为 十一月 12, 2025， [https://www.reddit.com/r/programming/comments/17507jf/how\_to\_avoid\_pybind11\_and\_write\_5x\_faster\_cpython/](https://www.reddit.com/r/programming/comments/17507jf/how_to_avoid_pybind11_and_write_5x_faster_cpython/)
28. Banana Pi Rockchip RKNN SDK quick start Guide \- BananaPi Docs, 访问时间为 十一月 12, 2025， [https://docs.banana-pi.org/en/BPI-CM5\_Pro/BananaPi\_BPI-CM5\_Pro/Rockchip\_RKNN\_Guide](https://docs.banana-pi.org/en/BPI-CM5_Pro/BananaPi_BPI-CM5_Pro/Rockchip_RKNN_Guide)
29. YoloV5 NPU for the RK3566/68/88 \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/Qengineering/YoloV5-NPU](https://github.com/Qengineering/YoloV5-NPU)
30. 02 Rockchip RKNPU User Guide RKNN SDK V2.0.0beta0 EN | PDF \- Scribd, 访问时间为 十一月 12, 2025， [https://www.scribd.com/document/774992182/02-Rockchip-RKNPU-User-Guide-RKNN-SDK-V2-0-0beta0-EN](https://www.scribd.com/document/774992182/02-Rockchip-RKNPU-User-Guide-RKNN-SDK-V2-0-0beta0-EN)
31. Passing QImage data from a text file or data stream \- Qt Forum, 访问时间为 十一月 12, 2025， [https://forum.qt.io/topic/80810/passing-qimage-data-from-a-text-file-or-data-stream](https://forum.qt.io/topic/80810/passing-qimage-data-from-a-text-file-or-data-stream)
32. How to cast OpenCV cv::Mat as Void pointer? \- c++ \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/69425785/how-to-cast-opencv-cvmat-as-void-pointer](https://stackoverflow.com/questions/69425785/how-to-cast-opencv-cvmat-as-void-pointer)
33. How to convert cv::Mat to void \- c++ \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/61042167/how-to-convert-cvmat-to-void](https://stackoverflow.com/questions/61042167/how-to-convert-cvmat-to-void)
34. Rockchip User Guide RKNN API \- Index of /, 访问时间为 十一月 12, 2025， [https://repo.rock-chips.com/rk1808/doc/Rockchip\_User\_Guide\_RKNN\_API\_EN.pdf](https://repo.rock-chips.com/rk1808/doc/Rockchip_User_Guide_RKNN_API_EN.pdf)
35. Rockchip\_User\_Guide\_RKNN\_A, 访问时间为 十一月 12, 2025， [https://repo.rock-chips.com/rk1808/rknn-api/Rockchip\_User\_Guide\_RKNN\_API\_V1.4.0\_EN.pdf](https://repo.rock-chips.com/rk1808/rknn-api/Rockchip_User_Guide_RKNN_API_V1.4.0_EN.pdf)
36. yuunnn-w/rknn-cpp-yolo: This project implements YOLOv11 inference on the RK3588 platform using the RKNN framework. With deep optimization of the official code and RGA hardware acceleration for image preprocessing, it achieves a stable 25 FPS for YOLOv11s without overclocking or core binding, showcasing efficient real-time object detection for \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/yuunnn-w/rknn-cpp-yolo](https://github.com/yuunnn-w/rknn-cpp-yolo)
37. 3\. RKNN SDK Quick Start Guide | ArmSoM docs, 访问时间为 十一月 12, 2025， [https://docs.armsom.org/advanced-manual/rknn-sdk](https://docs.armsom.org/advanced-manual/rknn-sdk)
38. rockchip-linux/mpp: Media Process Platform (MPP) module \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/rockchip-linux/mpp](https://github.com/rockchip-linux/mpp)
39. Rockchip RK3566 features and highlights, 访问时间为 十一月 12, 2025， [https://www.96rocks.com/blog/2020/10/21/rockchip-rk3566-highlights/?ref=rgb](https://www.96rocks.com/blog/2020/10/21/rockchip-rk3566-highlights/?ref=rgb)
40. Rockchip RK3566 Datasheet \- Boardcon, 访问时间为 十一月 12, 2025， [https://www.boardcon.com/download/Rockchip\_RK3566\_Datasheet\_V1.1.pdf](https://www.boardcon.com/download/Rockchip_RK3566_Datasheet_V1.1.pdf)
41. Rockchip SoC Comparison \- Boardcon Embedded Design, 访问时间为 十一月 12, 2025， [https://www.armdesigner.com/article.php?id=278](https://www.armdesigner.com/article.php?id=278)
42. RK3588 \- Reverse engineering the RKNN (Rockchip Neural Processing Unit) \- Tiny Devices, 访问时间为 十一月 12, 2025， [http://jas-hacks.blogspot.com/2024/02/rk3588-reverse-engineering-rknn.html](http://jas-hacks.blogspot.com/2024/02/rk3588-reverse-engineering-rknn.html)
43. 2\. NPU — Firefly Wiki, 访问时间为 十一月 12, 2025， [https://wiki.t-firefly.com/en/ROC-RK3588S-PC/usage\_npu.html](https://wiki.t-firefly.com/en/ROC-RK3588S-PC/usage_npu.html)
44. Build for RKNN — mmdeploy 0.14.0 documentation, 访问时间为 十一月 12, 2025， [https://mmdeploy.readthedocs.io/en/0.x/01-how-to-build/rockchip.html](https://mmdeploy.readthedocs.io/en/0.x/01-how-to-build/rockchip.html)
45. Rockchip RK3588 perf \#722 \- ggml-org/llama.cpp \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/ggml-org/llama.cpp/issues/722](https://github.com/ggml-org/llama.cpp/issues/722)
46. 4\. RKNN API — Firefly Wiki, 访问时间为 十一月 12, 2025， [https://wiki.t-firefly.com/en/3399pro\_npu/npu\_rknn\_api.html](https://wiki.t-firefly.com/en/3399pro_npu/npu_rknn_api.html)
47. YoloV5 NPU multithread for the RK3566/68/88 (200 FPS) \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/Qengineering/YoloV5-NPU-Multithread](https://github.com/Qengineering/YoloV5-NPU-Multithread)
48. Mainline Hardware Decoding \- PINE64 Wiki, 访问时间为 十一月 12, 2025， [https://wiki.pine64.org/wiki/Mainline\_Hardware\_Decoding](https://wiki.pine64.org/wiki/Mainline_Hardware_Decoding)
49. RK3566, RK3576, and RK3588 compared : r/RockchipNPU \- Reddit, 访问时间为 十一月 12, 2025， [https://www.reddit.com/r/RockchipNPU/comments/1la7xdr/rk3566\_rk3576\_and\_rk3588\_compared/](https://www.reddit.com/r/RockchipNPU/comments/1la7xdr/rk3566_rk3576_and_rk3588_compared/)
50. rockchip-linux/rknpu2 \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/rockchip-linux/rknpu2](https://github.com/rockchip-linux/rknpu2)
51. 01 Rockchip RKNPU Quick Start RKNN SDK V1.6.0 EN | PDF \- Scribd, 访问时间为 十一月 12, 2025， [https://www.scribd.com/document/779317081/01-Rockchip-RKNPU-Quick-Start-RKNN-SDK-V1-6-0-EN](https://www.scribd.com/document/779317081/01-Rockchip-RKNPU-Quick-Start-RKNN-SDK-V1-6-0-EN)
52. Rockchip RKNN Model Zoo Tutorial \- AvionChip, 访问时间为 十一月 12, 2025， [https://avionchip.com/rockchip-rknn-model-zoo-tutorial/](https://avionchip.com/rockchip-rknn-model-zoo-tutorial/)
53. RKMPP RK3566 & Immich GPU Acceleration : r/OrangePI \- Reddit, 访问时间为 十一月 12, 2025， [https://www.reddit.com/r/OrangePI/comments/1lqdh32/rkmpp\_rk3566\_immich\_gpu\_acceleration/](https://www.reddit.com/r/OrangePI/comments/1lqdh32/rkmpp_rk3566_immich_gpu_acceleration/)
54. RKLLM Installation \- Radxa Docs, 访问时间为 十一月 12, 2025， [https://docs.radxa.com/en/rock5/rock5b/app-development/rkllm\_install](https://docs.radxa.com/en/rock5/rock5b/app-development/rkllm_install)
55. Add the new RKNPU kernel version 0.9.8 · Issue \#266 · armbian/linux-rockchip \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/armbian/linux-rockchip/issues/266](https://github.com/armbian/linux-rockchip/issues/266)
56. \[HW Accel Support\]:VPU Hardware Decoding Fails on RK3566 for Frigate, but Works for Jellyfin \#18878 \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/blakeblackshear/frigate/discussions/18878](https://github.com/blakeblackshear/frigate/discussions/18878)
57. How to upgrade rknpu on orange pi 5 max : r/RockchipNPU \- Reddit, 访问时间为 十一月 12, 2025， [https://www.reddit.com/r/RockchipNPU/comments/1i2lorn/how\_to\_upgrade\_rknpu\_on\_orange\_pi\_5\_max/](https://www.reddit.com/r/RockchipNPU/comments/1i2lorn/how_to_upgrade_rknpu_on_orange_pi_5_max/)
58. Update RKNPU Driver 0.9.8 for Orange Pi 5 Plus · Issue \#7254 · MichaIng/DietPi \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/MichaIng/DietPi/issues/7254](https://github.com/MichaIng/DietPi/issues/7254)
59. Feature Request: Add rknpu version 0.9.8 · Issue \#1093 · Joshua-Riek/ubuntu-rockchip, 访问时间为 十一月 12, 2025， [https://github.com/Joshua-Riek/ubuntu-rockchip/issues/1093](https://github.com/Joshua-Riek/ubuntu-rockchip/issues/1093)
60. 1\. Compile Linux5.10 firmware — Firefly Wiki, 访问时间为 十一月 12, 2025， [https://wiki.t-firefly.com/en/ROC-RK3566-PC/linux\_compile\_linux5.10.html](https://wiki.t-firefly.com/en/ROC-RK3566-PC/linux_compile_linux5.10.html)
61. Rockchip Developer Guide Linux Software En | PDF \- Scribd, 访问时间为 十一月 12, 2025， [https://www.scribd.com/document/927338308/Rockchip-Developer-Guide-Linux-Software-En](https://www.scribd.com/document/927338308/Rockchip-Developer-Guide-Linux-Software-En)
62. RKNN \- LUCKFOX WIKI, 访问时间为 十一月 12, 2025， [https://wiki.luckfox.com/Luckfox-Pico-Plus-Mini/RKNN/](https://wiki.luckfox.com/Luckfox-Pico-Plus-Mini/RKNN/)
63. airockchip/rknn\_model\_zoo \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/airockchip/rknn\_model\_zoo](https://github.com/airockchip/rknn_model_zoo)
64. Exported RKNN/ONNX model only has 1 output class instead of 2 \- Discussion \- Ultralytics, 访问时间为 十一月 12, 2025， [https://community.ultralytics.com/t/exported-rknn-onnx-model-only-has-1-output-class-instead-of-2/1223](https://community.ultralytics.com/t/exported-rknn-onnx-model-only-has-1-output-class-instead-of-2/1223)
65. RKNN \- LUCKFOX WIKI, 访问时间为 十一月 12, 2025， [https://wiki.luckfox.com/Luckfox-Pico-Plus-Mini/RKNN](https://wiki.luckfox.com/Luckfox-Pico-Plus-Mini/RKNN)
66. r/RockchipNPU \- Reddit, 访问时间为 十一月 12, 2025， [https://www.reddit.com/r/RockchipNPU/](https://www.reddit.com/r/RockchipNPU/)
67. RKNN Ultralytics YOLOv11 \- Radxa Docs, 访问时间为 十一月 12, 2025， [https://docs.radxa.com/en/rock5/rock5b/app-development/rknn\_ultralytics](https://docs.radxa.com/en/rock5/rock5b/app-development/rknn_ultralytics)
68. RockChip RK3566-ROC-PC Firefly (Quad-core 64-bit Cortex-A55) \- The Linux Channel, 访问时间为 十一月 12, 2025， [https://thelinuxchannel.org/2025/03/rockchip-rk3566-roc-pc-firefly-quad-core-64-bit-cortex-a55/](https://thelinuxchannel.org/2025/03/rockchip-rk3566-roc-pc-firefly-quad-core-64-bit-cortex-a55/)
69. Model Deviation issues on RK3566. ONNX versus RKNN : r/RockchipNPU \- Reddit, 访问时间为 十一月 12, 2025， [https://www.reddit.com/r/RockchipNPU/comments/1fw165z/model\_deviation\_issues\_on\_rk3566\_onnx\_versus\_rknn/](https://www.reddit.com/r/RockchipNPU/comments/1fw165z/model_deviation_issues_on_rk3566_onnx_versus_rknn/)
70. Rockchip User Guide RKNN API: Classification Level: Top Secret Secret Internal Public ( ), 访问时间为 十一月 12, 2025， [https://www.scribd.com/document/585013131/Rockchip-User-Guide-RKNN-API-V1-7-1-EN](https://www.scribd.com/document/585013131/Rockchip-User-Guide-RKNN-API-V1-7-1-EN)
71. YoloV8 NPU for the RK3566/68/88 \- GitHub, 访问时间为 十一月 12, 2025， [https://github.com/Qengineering/YoloV8-NPU](https://github.com/Qengineering/YoloV8-NPU)
72. Linking to Shared Library in Qt \- c++ \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/7182229/linking-to-shared-library-in-qt](https://stackoverflow.com/questions/7182229/linking-to-shared-library-in-qt)
73. Thread: Linking a custom shared library \- Qt Centre, 访问时间为 十一月 12, 2025， [https://www.qtcentre.org/threads/47774-Linking-a-custom-shared-library](https://www.qtcentre.org/threads/47774-Linking-a-custom-shared-library)
74. Is this an appropriate time to use QThread with QProcess? \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/43766559/is-this-an-appropriate-time-to-use-qthread-with-qprocess](https://stackoverflow.com/questions/43766559/is-this-an-appropriate-time-to-use-qthread-with-qprocess)
75. Thread: Execute a QProcess instead of a QThread \- Qt Centre, 访问时间为 十一月 12, 2025， [https://www.qtcentre.org/threads/70061-Execute-a-QProcess-instead-of-a-QThread](https://www.qtcentre.org/threads/70061-Execute-a-QProcess-instead-of-a-QThread)
76. using QT Process or threads to run functions? \- c++ \- Stack Overflow, 访问时间为 十一月 12, 2025， [https://stackoverflow.com/questions/66121444/using-qt-process-or-threads-to-run-functions](https://stackoverflow.com/questions/66121444/using-qt-process-or-threads-to-run-functions)
77. The Eight Rules of Multithreaded Qt \- KDAB, 访问时间为 十一月 12, 2025， [https://www.kdab.com/the-eight-rules-of-multithreaded-qt/](https://www.kdab.com/the-eight-rules-of-multithreaded-qt/)
78. Is it safe to have QThreads launching QProcesses \- Qt Forum, 访问时间为 十一月 12, 2025， [https://forum.qt.io/topic/86703/is-it-safe-to-have-qthreads-launching-qprocesses](https://forum.qt.io/topic/86703/is-it-safe-to-have-qthreads-launching-qprocesses)
79. Pass back QImage from thread repeatedly, deletes, proper way \- Qt Centre, 访问时间为 十一月 12, 2025， [https://www.qtcentre.org/threads/67266-Pass-back-QImage-from-thread-repeatedly-deletes-proper-way](https://www.qtcentre.org/threads/67266-Pass-back-QImage-from-thread-repeatedly-deletes-proper-way)
80. Passing Mat in form of signal \- Qt Forum, 访问时间为 十一月 12, 2025， [https://forum.qt.io/topic/89034/passing-mat-in-form-of-signal](https://forum.qt.io/topic/89034/passing-mat-in-form-of-signal)
81. What is the best practise for passing cv::Mats around edit \- OpenCV Q\&A Forum, 访问时间为 十一月 12, 2025， [https://answers.opencv.org/question/14798/what-is-the-best-practise-for-passing-cvmats-around/](https://answers.opencv.org/question/14798/what-is-the-best-practise-for-passing-cvmats-around/)
