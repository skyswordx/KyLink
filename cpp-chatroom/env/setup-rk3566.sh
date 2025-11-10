#!/bin/bash

# Source this script to set up the RK3566 cross-compilation environment.
# Example: source env/setup-rk3566.sh

export RK3566_SDK=/home/circlemoon/SDK/firefly_rk3588_SDK
export RK3566_TOOLCHAIN="$RK3566_SDK/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu"
export RK3566_SYSROOT=/home/circlemoon/sysroots/rk3566

export PATH="$RK3566_TOOLCHAIN/bin:$PATH"
export PKG_CONFIG_SYSROOT_DIR="$RK3566_SYSROOT"
export PKG_CONFIG_LIBDIR="$RK3566_SYSROOT/usr/lib/pkgconfig:$RK3566_SYSROOT/usr/share/pkgconfig"
