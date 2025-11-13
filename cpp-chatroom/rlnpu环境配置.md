skyswordx: 请你帮我解压在仓库目录下的Reference目录下的rknn相关tgz文件，我想解压后的文件夹名字直接是版本号，不用带上日期和extracted

Ran terminal command: cd /home/circlemoon/kylin/Reference && tar -tf rknn-toolkit2-v2.3.0-2024-11-08.tgz | head

Ran terminal command: cd /home/circlemoon/kylin/Reference && mkdir -p rknn-toolkit2-v2.3.0 && tar -xzf rknn-toolkit2-v2.3.0-2024-11-08.tgz -C rknn-toolkit2-v2.3.0 --strip-components=1

Ran terminal command: cd /home/circlemoon/kylin/Reference && mkdir -p rknn_model_zoo-v2.3.0 && tar -xzf rknn_model_zoo-v2.3.0-2024-11-08.tgz -C rknn_model_zoo-v2.3.0 --strip-components=1


解压完成：rknn-toolkit2-v2.3.0 和 rknn_model_zoo-v2.3.0 里已经是剥掉日期后的内容。压缩包原件保持在原处。需要的话可以删除 tgz 或继续处理。

skyswordx: 请你根据这个readme，准备我WSL的交叉编译环境，主要是so和头文件要放好到对应的位置？可以参考我的setup找到我的SYSROOT，然后我的板端是不是只需要复制lib,so即可了


GitHub Copilot: Ran terminal command: sudo install -Dm755 /home/circlemoon/kylin/Reference/rknn-toolkit2-v2.3.0/rknpu2/runtime/Linux/librknn_api/aarch64/librknnrt.so /home/circlemoon/sysroots/rk3566/usr/lib/aarch64-linux-gnu/librknnrt.so

Ran terminal command: sudo ln -sf ../lib/aarch64-linux-gnu/librknnrt.so /home/circlemoon/sysroots/rk3566/usr/lib/librknnrt.so

Ran terminal command: sudo install -d /home/circlemoon/sysroots/rk3566/usr/include/rknn

Ran terminal command: sudo install -m644 /home/circlemoon/kylin/Reference/rknn-toolkit2-v2.3.0/rknpu2/runtime/Linux/librknn_api/include/rknn_*.h /home/circlemoon/sysroots/rk3566/usr/include/rknn/

Ran terminal command: ls -l /home/circlemoon/sysroots/rk3566/usr/lib/aarch64-linux-gnu/librknnrt.so

Ran terminal command: ls -l /home/circlemoon/sysroots/rk3566/usr/lib/librknnrt.so

Ran terminal command: ls -l /home/circlemoon/sysroots/rk3566/usr/include/rknn

Ran terminal command: source /home/circlemoon/kylin/cpp-chatroom/env/setup-rk3566.sh && echo $RK3566_SYSROOT

Ran terminal command: source /home/circlemoon/kylin/cpp-chatroom/env/setup-rk3566.sh && aarch64-linux-gnu-g++ -print-sysroot

- 已按 README 要求，将 `librknnrt.so` 复制到 `$RK3566_SYSROOT/usr/lib/aarch64-linux-gnu/`，并在 `$RK3566_SYSROOT/usr/lib/` 建立符号链接，当前权限/大小如下：`-rwxr-xr-x 1 root root 7259064 … librknnrt.so`。  
- 创建 `$RK3566_SYSROOT/usr/include/rknn` 并放入 `rknn_api.h`、`rknn_custom_op.h`、`rknn_matmul_api.h`，确认均为 `root:root`、`0644`。  
- `source env/setup-rk3566.sh` 后，`RK3566_SYSROOT` 输出为 rk3566，说明环境变量正确。`aarch64-linux-gnu-g++ -print-sysroot` 当前返回 `/`，交叉编译时请确保 CMake toolchain 或编译命令显式指定 `--sysroot=$RK3566_SYSROOT`（已有的 `cmake/toolchains/rk3566.cmake` 通常会处理）。  
- 开始交叉编译前，请重新运行 `source env/setup-rk3566.sh`；若还缺少 `pkg-config` 支持，可在命令行加 `PKG_CONFIG_SYSROOT_DIR` 与 `PKG_CONFIG_LIBDIR`（脚本已设）。  
- 板端只需拷贝 `librknnrt.so` 到 lib 或 aarch64-linux-gnu，然后执行 `sudo ldconfig`；头文件仅留在 WSL 侧即可。  
- 下一步可再次运行 `cmake -S . -B build-rk3566 ...`，验证交叉编译链是否就绪。

skyswordx: 请你生成一份详细的命令指示表，指示如何板端只需拷贝 librknnrt.so 到 lib 或 aarch64-linux-gnu，然后执行 sudo ldconfig，供我在板端进行复制粘贴

GitHub Copilot: **板端部署流程**

- **准备**：把 `librknnrt.so` 先拷到开发机的某个临时目录（示例用 `~/tmp/`），然后用 `scp` 传到板端。例如：
  ```bash
  scp ~/tmp/librknnrt.so <board_user>@<board_ip>:/tmp/
  ```
- **放置到 /usr/lib**（典型做法，若系统 aarch64-linux-gnu 已存在也可放那）：
  ```bash
  ssh <board_user>@<board_ip>
  sudo install -Dm755 /tmp/librknnrt.so /usr/lib/librknnrt.so
  sudo ldconfig
  ```
- **若更适合放入多架构目录**（如 Debian/Ubuntu 派生常见 aarch64-linux-gnu）：
  ```bash
  ssh <board_user>@<board_ip>
  sudo install -Dm755 /tmp/librknnrt.so /usr/lib/aarch64-linux-gnu/librknnrt.so
  sudo ln -sf ../lib/aarch64-linux-gnu/librknnrt.so /usr/lib/librknnrt.so   # 可选，提供兼容路径
  sudo ldconfig
  ```
- **验证动态库是否正确注册**（任意路径）：
  ```bash
  ldconfig -p | grep librknnrt
  ```
- **后续**：应用运行前确认新库已被加载（可用 `ldd your_app` 检查）。