<div align="center">

[简体中文](README.md) | **English**

![NPU Dynamic Inference Demo](assets-of-README/image-demo.png)

# KyLink

An edge AI communication and video streaming system on RK3566 + Ubuntu Kylin (FeiQ / NPU / GStreamer / LED Driver)

[![Platform](https://img.shields.io/badge/Platform-RK3566-blue?style=flat-square)](https://wiki.t-firefly.com/ROC-RK3566-PC/)
[![OS](https://img.shields.io/badge/OS-Ubuntu%20Kylin-E95420?style=flat-square)](https://ubuntukylin.com/)
[![Runtime](https://img.shields.io/badge/Runtime-RKNN%20%2B%20RGA%20%2B%20MPP-purple?style=flat-square)](https://github.com/rockchip-linux)
[![Framework](https://img.shields.io/badge/Framework-Qt5%20%2B%20GStreamer-00A98F?style=flat-square)](https://gstreamer.freedesktop.org/)
[![Protocol](https://img.shields.io/badge/Protocol-FeiQ%20UDP%2FTCP-orange?style=flat-square)](./cpp-chatroom/feiqlib)
[![Build](https://img.shields.io/badge/Build-CMake%20%2B%20Ninja-red?style=flat-square)](./cpp-chatroom/CMakeLists.txt)
[![License](https://img.shields.io/badge/License-GPLv3-green?style=flat-square)](LICENSE)

---

</div>

## Overview

KyLink is an embedded systems project deployed on RK3566 + Ubuntu Kylin, focused on end-to-end validation of LAN messaging, video streaming, NPU inference acceleration, and board-level driver integration.

Its main application is `cpp-chatroom`: based on FeiQ/IPMsg communication, it extends cross-platform builds, video stream sending/receiving, NPU detection pipeline, and runtime performance analysis. The repository also keeps `my_kylin_led_driver` as a BSP/kernel-driver practice module.

## Key Features

- LAN instant messaging via FeiQ/IPMsg (UDP 2425 + TCP file transfer)
- Cross-platform build support for Ubuntu Kylin desktop and RK3566 cross-compilation
- UDP video streaming with chunking/reassembly (default port 2426), optimized for low latency
- NPU inference pipeline integrating RKNN + RGA + MPP for real-time video detection
- Runtime diagnostics with NPU/RGA/MPP probe scripts
- Board-level LED character driver and usage notes

## Repository Layout

```text
KyLink/
├── cpp-chatroom/               # Main app: Qt UI + FeiQ + video + NPU
│   ├── src/                    # Business logic
│   ├── include/                # Headers
│   ├── feiqlib/                # FeiQ protocol stack
│   ├── env/                    # RK3566 cross-compilation env scripts
│   ├── docs/                   # Change logs / archival notes
│   └── GNUmakefile             # make desktop / make rk3566
├── my_kylin_led_driver/        # LED driver and kernel module artifacts
├── Reference/                  # Reference implementations and protocol assets
├── npu_info_explorer.sh        # NPU probe
├── rga_info_explorer.sh        # RGA probe
├── mpp_info_explorer.sh        # MPP probe
└── LICENSE                     # GPLv3
```

## Performance Snapshot (with Test Conditions)

### Model & Runtime Configuration

| Item | Configuration |
|---|---|
| Default model file | Runtime: `cpp-chatroom/npu_assets/yolov5s_relu.rknn`; repository asset: `cpp-chatroom/rknpu_resources/rk3566_model/yolov5s_relu.rknn` |
| Model input tensor | `(1, 3, 640, 640)` (from `performance_snapshot_20251227_060820.log`) |
| Inference data type | Input `UINT8`, major backbone ops `INT8` (visible in op-level logs) |
| Quantization | RKNN quantized tensors (`scale + zp` affine dequantization in `postprocess.cpp`) |
| Post-process thresholds | `BOX_THRESH=0.25`, `NMS_THRESH=0.45` |
| Camera input | `image/jpeg, 640x480` (GStreamer caps) |
| Preprocess path | `NV12 -> RGA letterbox -> 640x640 RGB -> RKNN` |
| NPU driver version | `RKNPU driver v0.9.3` (from performance snapshot log) |

### Observed Metrics (Two Snapshots on 2025-12-27)

| Metric | Observed Value | Source |
|---|---|---|
| End-to-end latency (avg) | `69.6 ms` / `80.5 ms` | `performance_snapshot_20251227_060820.log`, `...060846.log` |
| NPU stage latency (5-frame window) | `52.6–58.0 ms` | `NPU(μs)` columns in both snapshots |
| Preprocess latency (5-frame window) | `1.8–4.1 ms` | `预处理(μs)` columns in both snapshots |
| Video receive FPS | `~15 FPS` | `cpp-chatroom/docs/留档-20251225-跨平台视频流协议实现.md` |
| Video frame drops | `0–3 frames` | same as above |
| Detection throughput (project archive) | `11–19 FPS` | upper-level project archive / stage notes |
| Memory after C++ refactor | `81.56 MB` | upper-level project archive |

> Note: Snapshot results are affected by model version, input resolution, quantization parameters, RGA/MPP load, and system thermal conditions.

## Quick Start

### 1) Build desktop version (Ubuntu/Linux x64)

```bash
cd cpp-chatroom
make desktop
```

### 2) Cross-compile for RK3566

```bash
cd cpp-chatroom
source env/setup-rk3566.sh
make rk3566
```

### 3) Show build help

```bash
cd cpp-chatroom
make help
```

## Runtime Probes

Run these scripts from the `KyLink` root:

```bash
bash npu_info_explorer.sh
bash rga_info_explorer.sh
bash mpp_info_explorer.sh
```


## Acknowledgements

- FeiQ/IPMsg Linux implementation reference: [`uenigmas/linux_feiq`](https://github.com/uenigmas/linux_feiq)
- Historical reference code in this repo: `Reference/linux_feiq-master`

## License

This project is licensed under [GNU GPL v3](LICENSE).
