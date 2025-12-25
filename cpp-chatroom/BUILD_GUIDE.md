# FeiQ Chatroom 跨平台构建指南

本项目支持两种构建模式：

| 模式 | 平台 | 特性 |
|------|------|------|
| **桌面版** | Ubuntu/Linux x64 | 基础聊天功能，无NPU加速 |
| **RK3566版** | 麒麟/嵌入式 aarch64 | 完整功能，包含NPU YOLO检测 |

---

## 快速开始

### 方式一：使用 Makefile Wrapper（推荐）

```bash
# 构建桌面版
make desktop

# 构建 RK3566 版（需先配置环境）
source env/setup-rk3566.sh
make rk3566

# 查看帮助
make help
```

### 方式二：直接使用 CMake

#### 桌面版构建

```bash
mkdir build-desktop && cd build-desktop
cmake -DBUILD_DESKTOP=ON -G Ninja ..
ninja
```

#### RK3566 交叉编译

```bash
source env/setup-rk3566.sh
mkdir build-rk3566 && cd build-rk3566
cmake \
    -DBUILD_RK3566=ON \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/rk3566.cmake \
    -DCMAKE_PREFIX_PATH=$RK3566_SYSROOT/usr/lib/aarch64-linux-gnu/cmake \
    -G Ninja ..
ninja
```

---

## 构建选项说明

| CMake 选项 | 默认值 | 说明 |
|------------|--------|------|
| `BUILD_RK3566` | OFF | 启用 RK3566 模式 (NPU/RGA/MPP) |
| `BUILD_DESKTOP` | OFF | 启用桌面模式 (无硬件加速依赖) |

> 如果两个选项都未指定，CMake 会自动检测平台：
> - aarch64 架构 → RK3566 模式
> - 其他架构 → 桌面模式

---

## 依赖要求

### 桌面版
- Qt5 >= 5.12.8 (Core, Gui, Widgets, Network)
- CMake >= 3.16
- Ninja 或 Make

```bash
# Ubuntu 安装
sudo apt install qt5-default qtbase5-dev cmake ninja-build
```

### RK3566 版（额外）
- GStreamer 1.0
- RGA (librga)
- Rockchip MPP
- RKNN Runtime
- OpenCV

详见 [RK3566交叉编译.md](RK3566交叉编译.md)

---

## 输出文件

| 模式 | 可执行文件位置 |
|------|----------------|
| 桌面版 | `build-desktop/bin/FeiQChatroom` |
| RK3566版 | `build-rk3566/bin/FeiQChatroom` |

---

## 常见问题

### Q: 桌面版缺少 GStreamer？
A: 桌面版不需要 GStreamer，如遇到错误请确认使用了 `-DBUILD_DESKTOP=ON`

### Q: RK3566 编译报错找不到 RKNN？
A: 请先执行 `source env/setup-rk3566.sh` 配置 sysroot

### Q: 如何清理构建？
```bash
make clean          # 清理所有
make clean-desktop  # 仅清理桌面
make clean-rk3566   # 仅清理 RK3566
```
