# FeiQ Chatroom - C++ 版本

这是一个基于 Qt5 和 C++ 实现的飞秋（FeiQ）聊天室应用程序，使用 IPMsg 协议进行通信。

## 项目结构

```
cpp-chatroom/
├── CMakeLists.txt          # CMake构建文件
├── cmake/                   # CMake工具链文件
│   └── aarch64-linux-gnu.cmake
├── include/                 # 头文件
│   ├── FeiqBackend.h       # 协议控制器，封装 feiqlib
│   ├── FeiqTypes.h         # UI 与协议层的数据结构
│   ├── MainWindow.h        # 主窗口
│   ├── ChatWindow.h        # 聊天窗口
│   └── GroupChatDialog.h   # 群发对话框
├── src/                     # 源文件
│   ├── main.cpp
│   ├── FeiqBackend.cpp
│   ├── MainWindow.cpp
│   ├── ChatWindow.cpp
│   ├── GroupChatDialog.cpp
│   ├── SettingsDialog.cpp
│   └── AboutDialog.cpp
├── feiqlib/                 # 来自 macOS 飞秋项目的协议与模型实现（参考 `feiq/feiqlib`）
└── ui/                      # UI文件（可选，当前使用代码创建UI）
```

## 依赖要求

- CMake >= 3.16
- Qt5 >= 5.12.8（包含以下组件：Core, Gui, Widgets, Network；当前构建验证使用 5.12.8）
- C++17 兼容的编译器

### Ubuntu 20.04 安装依赖

```bash
sudo apt-get update
sudo apt-get install cmake qt5-default qtbase5-dev qtbase5-dev-tools
```

### Windows 10/11

1. 安装 CMake: https://cmake.org/download/
2. 安装 Qt5: https://www.qt.io/download-qt-installer
   - 选择 Qt 5.12.8 或同系列 5.12.x 版本
   - 确保安装 Qt5Core, Qt5Gui, Qt5Widgets, Qt5Network 组件
3. 将 Qt 的 bin 目录添加到 PATH 环境变量

## 构建说明

### 1. 本机 Ubuntu 20.04 构建

```bash
cd cpp-chatroom
mkdir build
cd build
cmake ..
make
```

编译完成后，可执行文件位于 `build/bin/FeiQChatroom`

### 2. Windows 10/11 构建

使用 CMake GUI 或命令行：

```cmd
cd cpp-chatroom
mkdir build
cd build
cmake .. -G "MinGW Makefiles"  # 或 "Visual Studio 16 2019" 等
cmake --build .
```

或者使用 Qt Creator：
1. 打开 Qt Creator
2. 选择 File -> Open File or Project
3. 选择 `cpp-chatroom/CMakeLists.txt`
4. 配置构建套件（选择 Qt 5.12.8 或对应 5.12.x 套件）
5. 点击构建

### 3. 交叉编译到 RK3566 (aarch64-linux-gnu)

#### 准备工作

1. 安装交叉编译工具链（例如 Linaro GCC）：
```bash
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

2. 交叉编译 Qt5（如果还没有）：
   - 下载 Qt5 源码
   - 使用交叉编译工具链编译 Qt5
   - 安装到指定目录（例如 `/opt/qt5-aarch64`）

3. 编辑 `cmake/aarch64-linux-gnu.cmake`，设置正确的路径：
   - `CMAKE_SYSROOT`：目标系统的 sysroot
   - `Qt5_DIR`：交叉编译的 Qt5 安装路径

#### 构建步骤

```bash
cd cpp-chatroom
mkdir build-aarch64
cd build-aarch64
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-linux-gnu.cmake
make
```

编译完成后，可执行文件位于 `build-aarch64/bin/FeiQChatroom`

> 更详细的 Windows x64 与 RK3566 交叉编译准备流程，请查看 `CROSS_COMPILE_PREP.md`。

#### 部署到 RK3566

将编译好的可执行文件和 Qt5 库部署到目标设备：

```bash
# 复制可执行文件
scp build-aarch64/bin/FeiQChatroom user@rk3566:/path/to/target/

# 复制 Qt5 库（根据实际安装路径调整）
scp -r /opt/qt5-aarch64/lib/* user@rk3566:/path/to/target/lib/
```

## 功能特性

### 已实现功能

- ✅ 基于 `feiqlib` 的飞秋协议栈（UDP 广播、TCP 文件传输）
- ✅ UI/协议层彻底解耦，`FeiqBackend` 负责与协议交互
- ✅ 在线用户发现、点对点聊天、窗口抖动提示
- ✅ 文件发送与接收，自动提示保存位置
- ✅ 群发消息（简单广播）
- ✅ 内置回环测试用户（`127.0.0.2`），可在单机验证消息与文件流程

### 已知限制

- 文件传输进度目前仅通过状态文本提示
- 文件夹/图片特殊传输模式仍依赖对端兼容
- UI 样式为基础 Qt 组件，尚未进行主题化

## 网络协议

本实现基于 IPMsg/飞秋协议：

- **UDP 端口**: 2425（默认）
- **TCP 端口**: 动态分配（由系统自动选择）

### 协议消息格式

```
版本号:包编号:发送者:发送主机:命令:附加信息
```

例如：`1:12345:user1:host1:32:Hello`

## 故障排除

### 端口被占用

如果遇到端口绑定错误，请检查：
1. 是否有其他实例正在运行
2. 是否有其他程序占用 2425 端口

### Qt5 找不到

如果 CMake 找不到 Qt5：

```bash
# Ubuntu
export Qt5_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt5

# 或指定完整路径
cmake .. -DQt5_DIR=/path/to/qt5/lib/cmake/Qt5
```

### 交叉编译问题

1. 确保交叉编译工具链已正确安装
2. 确保 Qt5 已正确交叉编译
3. 检查 `cmake/aarch64-linux-gnu.cmake` 中的路径设置

## 许可证

请参考项目根目录的 LICENSE 文件。

## 开发说明

本项目是 Python 版本的 C++ 迁移，主要改进：

1. **性能优化**: C++ 实现相比 Python 版本有更好的性能
2. **多线程**: 文件传输在独立线程中运行，不阻塞 GUI
3. **错误处理**: 改进了文件传输的错误处理机制
4. **跨平台**: 支持 Ubuntu、Windows 和嵌入式 Linux（RK3566）

## 下一步开发

- [ ] 实现文件传输进度条
- [ ] 实现群发消息功能
- [ ] 优化 UI 样式
- [ ] 添加更多错误处理和日志记录

