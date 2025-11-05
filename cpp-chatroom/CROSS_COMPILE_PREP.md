## 交叉编译准备指南

本指南帮助你在 **WSL 环境** 中同时为两类目标平台准备交叉编译环境：

- Windows 10/11 x64
- RK3566（Kylin Ubuntu）

当前仓库的主机端 Ubuntu 通过 Qt 5.12.8 完成验证，下面的流程也以该版本为基准。请根据实际需求调整版本号或模块选择。

---

### 1. 通用准备

- 更新并安装构建基础工具：
  ```bash
  sudo apt update
  sudo apt install build-essential ninja-build python3 git pkg-config gperf bison flex
  ```
- 获取 Qt 5.12.8 源码包（或官方 `qt-everywhere-src-5.12.8.tar.xz`），解压到统一路径，例如 `/opt/qt-src-5.12.8`。
- 建议建立统一的日志/脚本目录，例如 `scripts/qt-build/`，便于分别记录两种交叉编译配置。
- 若需加速多次构建，可准备 ccache 或在外部磁盘放置 `qtbase` 构建缓存。

---

### 2. 面向 Windows x64（MinGW）

#### 2.1 准备工具链

- 安装 MinGW64 交叉编译器：
  ```bash
  sudo apt install mingw-w64 g++-mingw-w64-x86-64
  sudo update-alternatives --config x86_64-w64-mingw32-g++  # 选择 posix 线程模型
  ```
- 可选：安装 `nsis`、`zip` 等打包工具，方便后续生成安装包或压缩部署目录。

#### 2.2 构建 Qt

1. 建立独立构建与安装路径：
   ```bash
   mkdir -p /opt/qt-build-win64
   mkdir -p /opt/qt-5.12.8-windows
   cd /opt/qt-build-win64
   ```
2. 运行 configure（示例使用 Makefile，可按需加 `-ninja`）：
   ```bash
   /opt/qt-src-5.12.8/configure \
       -prefix /opt/qt-5.12.8-windows \
       -opensource -confirm-license \
       -platform linux-g++-64 \
       -xplatform win32-g++ \
       -device-option CROSS_COMPILE=x86_64-w64-mingw32- \
       -nomake tests -nomake examples \
       -opengl desktop \
       -skip qtwebengine
   ```
3. 编译并安装：
   ```bash
   cmake --build . --parallel   # 或 make / ninja
   cmake --install .
   ```
4. 记录安装结果：`/opt/qt-5.12.8-windows/bin/qmake -v` 确认版本。

#### 2.3 项目交叉编译

- 在仓库 `cmake/` 下新增 `x86_64-w64-mingw32.cmake`（示例）：
  ```cmake
  set(CMAKE_SYSTEM_NAME Windows)
  set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
  set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
  set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

  set(CMAKE_FIND_ROOT_PATH /opt/qt-5.12.8-windows)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  ```
- 构建命令：
  ```bash
  cmake -S /home/circlemoon/kylin/cpp-chatroom \
        -B /home/circlemoon/kylin/cpp-chatroom/build-win64 \
        -G "Ninja" \
        -DCMAKE_TOOLCHAIN_FILE=/home/circlemoon/kylin/cpp-chatroom/cmake/x86_64-w64-mingw32.cmake \
        -DQt5_DIR=/opt/qt-5.12.8-windows/lib/cmake/Qt5 \
        -DCMAKE_BUILD_TYPE=Release

  cmake --build /home/circlemoon/kylin/cpp-chatroom/build-win64 --parallel
  ```
- 部署：把 `build-win64/bin/FeiQChatroom.exe` 以及 `/opt/qt-5.12.8-windows/bin/windeployqt` 收集到的依赖打包，传输到 Windows 环境验证运行。

#### 2.4 静态链接（可选）

- 在 configure 中追加 `-static -static-runtime`，并准备 MinGW 的静态运行库。
- 注意遵守 Qt LGPL 许可：需向用户提供对象文件或完整构建脚本允许其重新链接。

---

### 3. 面向 RK3566（Kylin Ubuntu）

#### 3.1 准备工具链与 sysroot

- 安装 aarch64 交叉编译工具链：
  ```bash
  sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
  ```
- 从 RK3566 板端同步系统库（以 root 权限执行，或使用已有 sysroot）：
  ```bash
  rsync -a root@rk3566:/usr /opt/rk3566-sysroot/usr
  rsync -a root@rk3566:/lib /opt/rk3566-sysroot/lib
  rsync -a root@rk3566:/etc/ld.so.conf* /opt/rk3566-sysroot/etc/
  ```
- 确认板端 Qt 版本：
  ```bash
  qmake -v
  strings /opt/rk3566-sysroot/usr/lib/aarch64-linux-gnu/libQt5Core.so.5 | grep "Qt "
  ```
  若版本低于 5.12.8 或模块缺失，需自行交叉编译一套匹配版本的 Qt。

#### 3.2 构建 Qt（板端已有完整 Qt 可跳过）

1. 准备安装目录：`mkdir -p /opt/qt-5.12.8-rk3566`
2. 运行 configure（sysroot 示例）：
   ```bash
   /opt/qt-src-5.12.8/configure \
       -prefix /opt/qt-5.12.8-rk3566 \
       -opensource -confirm-license \
       -release \
       -platform linux-g++-64 \
       -device-option CROSS_COMPILE=aarch64-linux-gnu- \
       -device-option QT_QPA_DEFAULT_PLATFORM=linuxfb \
       -sysroot /opt/rk3566-sysroot \
       -nomake tests -nomake examples \
       -skip qtwebengine
   ```
3. 编译并安装（同上）。

#### 3.3 项目交叉编译

- 在 `cmake/aarch64-linux-gnu.cmake` 中补全：
  ```cmake
  set(CMAKE_SYSTEM_NAME Linux)
  set(CMAKE_SYSTEM_PROCESSOR aarch64)
  set(CMAKE_SYSROOT /opt/rk3566-sysroot)

  set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
  set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

  set(CMAKE_FIND_ROOT_PATH /opt/qt-5.12.8-rk3566 /opt/rk3566-sysroot)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
  ```
- 构建命令：
  ```bash
  cmake -S /home/circlemoon/kylin/cpp-chatroom \
        -B /home/circlemoon/kylin/cpp-chatroom/build-rk3566 \
        -G "Ninja" \
        -DCMAKE_TOOLCHAIN_FILE=/home/circlemoon/kylin/cpp-chatroom/cmake/aarch64-linux-gnu.cmake \
        -DQt5_DIR=/opt/qt-5.12.8-rk3566/lib/cmake/Qt5 \
        -DCMAKE_BUILD_TYPE=Release

  cmake --build /home/circlemoon/kylin/cpp-chatroom/build-rk3566 --parallel
  ```
- 部署：
  ```bash
  scp build-rk3566/bin/FeiQChatroom root@rk3566:/opt/feiqlab/
  ```
  若使用板端自带 Qt，确保 `LD_LIBRARY_PATH` 指向正确位置；如带自建 Qt，请一并同步 `/opt/qt-5.12.8-rk3566` 的 `lib`。

#### 3.4 运行与验证

- 登录板端，导出必要环境变量：
  ```bash
  export LD_LIBRARY_PATH=/opt/qt-5.12.8-rk3566/lib:$LD_LIBRARY_PATH
  export QT_QPA_PLATFORM=linuxfb  # 或 wayland/xcb，视桌面环境而定
  /opt/feiqlab/FeiQChatroom
  ```
- 如果使用系统自带 Qt，确保版本匹配并安装缺失的插件（如 `qtbase-plugins`、`qt5dxcb-plugin` 等）。

---

### 4. 验证流程建议

- **版本一致性**：保持主机、Windows、RK3566 上 Qt 主版本一致（5.12.x），减少 ABI 差异。
- **构建缓存**：不同目标使用各自的构建缓存目录，避免交叉污染。
- **日志记录**：将每次 configure 的参数写入脚本或 Markdown，方便排查。
- **部署测试**：
  - Windows：在原生 Windows 环境运行 `FeiQChatroom.exe` 并验证 `windeployqt` 结果。
  - RK3566：检查网络配置、显示后端（FB、Wayland、X11）是否匹配应用设置。

通过上述准备，即可在 WSL 中高效完成面向 Windows x64 与 RK3566 Kylin Ubuntu 的交叉编译。


