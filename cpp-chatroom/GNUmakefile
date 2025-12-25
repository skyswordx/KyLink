# =============================================================================
# FeiQ Chatroom - 跨平台构建 Makefile Wrapper
# =============================================================================
# 用法:
#   make desktop    - 构建桌面版 (Ubuntu x64)
#   make rk3566     - 交叉编译 RK3566 版
#   make clean      - 清理所有构建目录
#   make help       - 显示帮助信息
# =============================================================================

.PHONY: all desktop rk3566 clean help configure-desktop configure-rk3566

# 默认目标
all: help

# =============================================================================
# 桌面构建 (Ubuntu x64)
# =============================================================================
BUILD_DESKTOP := build-desktop
CMAKE_DESKTOP_ARGS := -DBUILD_DESKTOP=ON -DBUILD_RK3566=OFF

configure-desktop:
	@echo "========================================="
	@echo "配置桌面构建..."
	@echo "========================================="
	@mkdir -p $(BUILD_DESKTOP)
	cd $(BUILD_DESKTOP) && cmake $(CMAKE_DESKTOP_ARGS) -G Ninja ..

desktop: configure-desktop
	@echo "========================================="
	@echo "构建桌面版本..."
	@echo "========================================="
	cmake --build $(BUILD_DESKTOP) --parallel
	@echo "========================================="
	@echo "✅ 桌面版构建完成"
	@echo "可执行文件: $(BUILD_DESKTOP)/bin/FeiQChatroom"
	@echo "========================================="

# =============================================================================
# RK3566 交叉编译
# =============================================================================
BUILD_RK3566 := build-rk3566
CMAKE_RK3566_ARGS := -DBUILD_RK3566=ON -DBUILD_DESKTOP=OFF \
	-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/rk3566.cmake \
	-DCMAKE_PREFIX_PATH=$(RK3566_SYSROOT)/usr/lib/aarch64-linux-gnu/cmake

configure-rk3566:
	@echo "========================================="
	@echo "配置 RK3566 交叉编译..."
	@echo "========================================="
	@if [ -z "$$RK3566_SYSROOT" ]; then \
		echo "❌ 错误: 请先执行 'source env/setup-rk3566.sh'"; \
		exit 1; \
	fi
	@mkdir -p $(BUILD_RK3566)
	cd $(BUILD_RK3566) && cmake $(CMAKE_RK3566_ARGS) -G Ninja ..

rk3566: configure-rk3566
	@echo "========================================="
	@echo "构建 RK3566 版本..."
	@echo "========================================="
	cmake --build $(BUILD_RK3566) --parallel
	@echo "========================================="
	@echo "✅ RK3566 版本构建完成"
	@echo "可执行文件: $(BUILD_RK3566)/bin/FeiQChatroom"
	@file $(BUILD_RK3566)/bin/FeiQChatroom
	@echo "========================================="

# =============================================================================
# 清理
# =============================================================================
clean:
	@echo "清理构建目录..."
	rm -rf $(BUILD_DESKTOP) $(BUILD_RK3566)
	@echo "清理完成"

clean-desktop:
	rm -rf $(BUILD_DESKTOP)

clean-rk3566:
	rm -rf $(BUILD_RK3566)

# =============================================================================
# 帮助信息
# =============================================================================
help:
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════════════╗"
	@echo "║          FeiQ Chatroom - 跨平台构建系统                          ║"
	@echo "╠══════════════════════════════════════════════════════════════════╣"
	@echo "║ 构建目标:                                                        ║"
	@echo "║   make desktop     - 构建桌面版 (Ubuntu/Linux x64)               ║"
	@echo "║   make rk3566      - 交叉编译 RK3566 版 (需先 source 环境脚本)   ║"
	@echo "║                                                                  ║"
	@echo "║ 清理:                                                            ║"
	@echo "║   make clean       - 清理所有构建目录                            ║"
	@echo "║   make clean-desktop - 仅清理桌面构建                            ║"
	@echo "║   make clean-rk3566  - 仅清理 RK3566 构建                        ║"
	@echo "║                                                                  ║"
	@echo "║ RK3566 构建前置步骤:                                             ║"
	@echo "║   source env/setup-rk3566.sh                                     ║"
	@echo "║   make rk3566                                                    ║"
	@echo "╚══════════════════════════════════════════════════════════════════╝"
	@echo ""
