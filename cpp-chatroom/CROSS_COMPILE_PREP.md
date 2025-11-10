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

#### 3.1 操作顺序（速览）
- 在板端核对 Qt 运行库（Core/Gui/Widgets/Network）是否完备。
- 在板端打包 sysroot（`/usr`、`/lib`、`/etc/ld.so.conf*` 等），并通过 U 盘或其它介质拷贝到 WSL。
- 在 WSL 安装 aarch64 交叉工具链，解包 sysroot 至 `/opt/rk3566-sysroot`。
- 准备一套 Qt 5.12.8 aarch64 安装（交叉编译生成或沿用板端开发包）。
- 使用 `cmake/aarch64-linux-gnu.cmake` 进行 CMake 配置并构建项目。
- 将 `FeiQChatroom` 与 Qt 运行库传回板端，完成运行验证。

#### 3.2 板端准备

##### 3.2.1 核对 Qt 运行库
- 使用 `ldconfig -p | grep Qt5` 验证核心库是否存在。截图中可看到 `libQt5Core.so.5`、`libQt5Gui.so.5`、`libQt5Widgets.so.5`、`libQt5Network.so.5` 等均已注册，即满足当前工程的动态链接需求。
- 若缺少基础库，可安装发行版运行时包：
  ```bash
  sudo apt install libqt5core5a libqt5gui5 libqt5widgets5 libqt5network5
  ```
- 运行 `qmake -v` 确认 Qt 版本；使用 `ls -l /usr/lib/aarch64-linux-gnu/libQt5Core.so*` 查看符号链接是否指向 `.so.5.12.8` 等真实文件。
- 对已有可执行程序，可通过 `ldd /opt/feiqlab/FeiQChatroom | grep Qt5` 再次确认链接情况。
- `qtbase5-dev` 主要提供头文件与开发符号；当前仓库只需运行库即可执行。`QtCharts`、`QtDeclarative`、`QtMultimedia`、`QtSvg` 等模块暂未在代码中使用，无需额外安装 `qtcharts5-dev` 等包。

##### 3.2.2 无网络环境采集 sysroot
1. 在板端准备输出目录并创建 tar 包（保持 root 权限，保留符号链接与权限）：
   ```bash
   sudo tar -cpf /tmp/rk3566-sysroot-$(date +%Y%m%d).tar \
       --numeric-owner --xattrs --acls \
       -C / \
       usr \
       lib \
       etc/ld.so.conf \
       etc/ld.so.conf.d \
       etc/ld.so.cache
   ```
   如有自定义库，还可追加 `usr/local` 或其它目录。`tar` 会自动保留符号链接，无需额外处理。
2. 生成校验值便于回传后核对：
   ```bash
   sha256sum /tmp/rk3566-sysroot-*.tar
   ```
3. 将压缩包复制到 U 盘或 SD 卡（示例挂载点 `/media/usb`）：
   ```bash
   sudo mount /dev/sdX1 /media/usb
   sudo cp /tmp/rk3566-sysroot-*.tar /media/usb/
   sync
   sudo umount /media/usb
   ```
4. 若后续具备局域网和 SSH 条件，可改用 `rsync -a root@rk3566:/usr ...` 同步，命令与原流程兼容。

#### 3.3 WSL 端准备

##### 3.3.1 安装交叉工具链
```bash
sudo apt update
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

##### 3.3.2 导入 sysroot
1. 将板端导出的 `rk3566-sysroot-*.tar` 拷贝到 Windows 主机，再复制到 WSL 可访问的路径（例如 `/mnt/d/exports/`）。
2. 在 WSL 中解包：
   ```bash
   sudo mkdir -p /opt/rk3566-sysroot
   cd /opt/rk3566-sysroot
   sudo tar -xpf /mnt/d/exports/rk3566-sysroot-*.tar
   ```
3. 检查关键库（符号链接应完整）：
   ```bash
   ls -l /opt/rk3566-sysroot/usr/lib/aarch64-linux-gnu/libQt5Core.so*
   ls -l /opt/rk3566-sysroot/usr/lib/aarch64-linux-gnu/libQt5Gui.so*
   ```
4. 可选：添加一个便捷脚本 `scripts/env-rk3566.sh`，统一导出 `SYSROOT=/opt/rk3566-sysroot`、`PKG_CONFIG_PATH=${SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:$PKG_CONFIG_PATH` 等变量，方便后续构建。

#### 3.4 构建 Qt（板端已有完整 Qt 可跳过）

1. 准备安装目录：`mkdir -p /opt/qt-5.12.8-rk3566`
2. 运行 configure（指定 sysroot）：
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
3. 编译并安装：
   ```bash
   cmake --build . --parallel
   cmake --install .
   ```
4. 安装完成后确认 `Qt5Config.cmake` 位置：
   ```bash
   find /opt/qt-5.12.8-rk3566 -name Qt5Config.cmake
   ```
   若输出路径存在，即可用于 CMake 的 `Qt5_DIR` 参数。

#### 3.5 项目交叉编译

- 更新 `cmake/aarch64-linux-gnu.cmake`（如需，可追加 `set(CMAKE_PREFIX_PATH "/opt/qt-5.12.8-rk3566")` 与 `set(QT_QMAKE_EXECUTABLE "/opt/qt-5.12.8-rk3566/bin/qmake")`，保证 CMake 能找到 Qt）：
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
- 运行 CMake 生成并构建：
  ```bash
  cmake -S /home/circlemoon/kylin/cpp-chatroom \
        -B /home/circlemoon/kylin/cpp-chatroom/build-rk3566 \
        -G "Ninja" \
        -DCMAKE_TOOLCHAIN_FILE=/home/circlemoon/kylin/cpp-chatroom/cmake/aarch64-linux-gnu.cmake \
        -DQt5_DIR=/opt/qt-5.12.8-rk3566/lib/cmake/Qt5 \
        -DCMAKE_BUILD_TYPE=Release

  cmake --build /home/circlemoon/kylin/cpp-chatroom/build-rk3566 --parallel
  ```
- 构建完成后，可执行 `file build-rk3566/bin/FeiQChatroom`，确认目标为 `ELF 64-bit LSB executable, ARM aarch64`。
- 若需离线部署，可打包输出：
  ```bash
  tar -cpf build-rk3566.tar -C /home/circlemoon/kylin/cpp-chatroom/build-rk3566 bin lib
  ```

#### 3.6 运行与验证

- 将 `build-rk3566/bin/FeiQChatroom` 与所需 Qt 运行库拷贝回板端（无网络时可复用 U 盘传输）。
- 登录板端，导出必要环境变量：
  ```bash
  export LD_LIBRARY_PATH=/opt/qt-5.12.8-rk3566/lib:$LD_LIBRARY_PATH
  export QT_QPA_PLATFORM=linuxfb  # 或 wayland/xcb，视桌面环境而定
  /opt/feiqlab/FeiQChatroom
  ```
- 若使用系统自带 Qt，确保 `ldd /opt/feiqlab/FeiQChatroom` 中的依赖全部解析；如使用自建 Qt，请一并部署 `/opt/qt-5.12.8-rk3566` 下的插件与库。

---




### 3(). RK3566 交叉编译准备

#### 快速索引
- [环境准备](#环境准备)
- [获取 sysroot](#获取-sysroot)
- [环境变量与 PATH](#环境变量与-path)
- [CMake 工具链文件](#cmake-工具链文件)
- [配置与构建示例](#配置与构建示例)
- [常见问题](#常见问题)

#### 环境准备
- 主机：WSL2（Ubuntu 20.04/22.04 均可），确保 `cmake` ≥ 3.18、`ninja-build`、`rsync`、`pkg-config` 已安装。  
  ```bash
  sudo apt update
  sudo apt install build-essential cmake ninja-build pkg-config rsync
  ```
- SDK：`/home/circlemoon/SDK/firefly_rk3588_SDK`
- 工具链：`/home/circlemoon/SDK/firefly_rk3588_SDK/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu`
  - C 编译器：`.../bin/aarch64-none-linux-gnu-gcc`
  - C++ 编译器：`.../bin/aarch64-none-linux-gnu-g++`

##### 获取 sysroot

推荐从目标板 `rsync`：
  ```bash
  mkdir -p /home/circlemoon/kylin/sysroots/rk3566
  rsync -a root@<board-ip>:/lib/  /home/circlemoon/kylin/sysroots/rk3566/lib/
  rsync -a root@<board-ip>:/usr/  /home/circlemoon/kylin/sysroots/rk3566/usr/
  ```

也可以直接利用 SDK 里的 Ubuntu20.04.3LTS_Firefly_202510071336_rootfs.img 来准备交叉编译用的 sysroot，流程如下（以 WSL 为例，需要 `sudo` 权限）：

1. **创建挂载点与目标目录**
   ```bash
   mkdir -p /home/circlemoon/sysroots/rk3566-rootfs-mnt
   mkdir -p /home/circlemoon/sysroots/rk3566
   ```

2. **挂载 rootfs 镜像**
   ```bash
   sudo mount -o loop /home/circlemoon/SDK/firefly_rk3588_SDK/prebuilt_rootfs/Ubuntu20.04.3LTS_Firefly_202510071336_rootfs.img \
       /home/circlemoon/sysroots/rk3566-rootfs-mnt
   ```

3. **同步内容到 sysroot**
   ```bash
   sudo rsync -aHAX --delete \
       /home/circlemoon/sysroots/rk3566-rootfs-mnt/ \
       /home/circlemoon/sysroots/rk3566/
   ```

4. **卸载镜像**
   ```bash
   sudo umount /home/circlemoon/sysroots/rk3566-rootfs-mnt
   rmdir /home/circlemoon/sysroots/rk3566-rootfs-mnt
   ```


#### 环境变量与 PATH
在 `cpp-chatroom` 项目中新建 `env/setup-rk3566.sh`：
```bash
#!/bin/bash
export RK3566_SDK=/home/circlemoon/SDK/firefly_rk3588_SDK
export RK3566_TOOLCHAIN=$RK3566_SDK/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu
export RK3566_SYSROOT=/home/circlemoon/sysroots/rk3566

export PATH=$RK3566_TOOLCHAIN/bin:$PATH
export PKG_CONFIG_SYSROOT_DIR=$RK3566_SYSROOT
export PKG_CONFIG_LIBDIR=$RK3566_SYSROOT/usr/lib/pkgconfig:$RK3566_SYSROOT/usr/share/pkgconfig
```
使用时：
```bash
source env/setup-rk3566.sh
```

#### CMake 工具链文件
创建 `cmake/toolchains/rk3566.cmake`：
```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(NOT DEFINED ENV{RK3566_SYSROOT})
  message(FATAL_ERROR "请先 source env/setup-rk3566.sh")
endif()

set(RK3566_SYSROOT $ENV{RK3566_SYSROOT})
set(RK3566_TOOLCHAIN $ENV{RK3566_TOOLCHAIN})

set(CMAKE_SYSROOT ${RK3566_SYSROOT})
set(CMAKE_C_COMPILER ${RK3566_TOOLCHAIN}/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${RK3566_TOOLCHAIN}/bin/aarch64-none-linux-gnu-g++)

set(CMAKE_C_FLAGS_INIT "--sysroot=${CMAKE_SYSROOT}")
set(CMAKE_CXX_FLAGS_INIT "--sysroot=${CMAKE_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

#### 配置与构建示例
```bash
source env/setup-rk3566.sh
mkdir -p build-rk3566
cd build-rk3566
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/rk3566.cmake \
      -G Ninja \
      ..
ninja
```
编译完成后可用 `file <binary>` 确认为 `aarch64`。

#### 常见问题
- **头文件缺失**：确认 sysroot 中 `usr/include` 已同步，必要时从板端再拉取一次。
- **找不到 pkg-config 包**：检查 `PKG_CONFIG_LIBDIR` 是否指向 sysroot；必要时自定义 `.pc`。
- **链接失败 (GLIBC version)**：确保 sdk/板端 libc 版本一致；如板子较旧，优先使用板端 `rsync` 的 sysroot。
- **运行时库缺失**：把生成的可执行文件及依赖的 `.so` 一并拷贝到板子，例如 `/usr/local/bin` 与 `/usr/local/lib`。


### 4. 验证流程建议

- **版本一致性**：保持主机、Windows、RK3566 上 Qt 主版本一致（5.12.x），减少 ABI 差异。
- **构建缓存**：不同目标使用各自的构建缓存目录，避免交叉污染。
- **日志记录**：将每次 configure 的参数写入脚本或 Markdown，方便排查。
- **部署测试**：
  - Windows：在原生 Windows 环境运行 `FeiQChatroom.exe` 并验证 `windeployqt` 结果。
  - RK3566：检查网络配置、显示后端（FB、Wayland、X11）是否匹配应用设置。

通过上述准备，即可在 WSL 中高效完成面向 Windows x64 与 RK3566 Kylin Ubuntu 的交叉编译。


