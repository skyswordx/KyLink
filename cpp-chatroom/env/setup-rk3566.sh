#!/bin/bash

# Source this script to set up the RK3566 cross-compilation environment.
# Example: source env/setup-rk3566.sh

export RK3566_SDK=/home/circlemoon/SDK/firefly_rk3588_SDK
# 使用系统交叉编译器 (gcc-aarch64-linux-gnu / g++-aarch64-linux-gnu)
export RK3566_TOOLCHAIN=/usr
export RK3566_PREFIX=aarch64-linux-gnu
export RK3566_SYSROOT=/home/circlemoon/sysroots/rk3566

# 将系统交叉编译器放在 PATH 前面，避免使用 SDK 自带版本
export PATH="/usr/bin:$PATH"
export PKG_CONFIG_SYSROOT_DIR="$RK3566_SYSROOT"
export PKG_CONFIG_LIBDIR="$RK3566_SYSROOT/usr/lib/pkgconfig:$RK3566_SYSROOT/usr/share/pkgconfig:$RK3566_SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig"
