# CMake toolchain file for cross-compiling to aarch64-linux-gnu (RK3566)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake ..

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# 交叉编译工具链前缀
set(TOOLCHAIN_PREFIX "aarch64-linux-gnu")

# 设置C和C++编译器
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)

# 设置工具链路径（请根据实际情况修改）
# 假设工具链安装在 /opt/gcc-linaro-aarch64-linux-gnu 或类似路径
# set(CMAKE_FIND_ROOT_PATH /opt/gcc-linaro-aarch64-linux-gnu)

# 设置sysroot（请根据实际情况修改）
# 如果您的交叉编译工具链有sysroot，请取消注释并设置正确的路径
# set(CMAKE_SYSROOT /opt/sysroot-aarch64-linux-gnu)

# 设置find程序只在sysroot中查找
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Qt5 交叉编译配置
# 假设Qt5已交叉编译并安装在 /opt/qt5-aarch64
# 请根据实际情况修改以下路径
# set(Qt5_DIR /opt/qt5-aarch64/lib/cmake/Qt5)
# set(QT_QMAKE_EXECUTABLE /opt/qt5-aarch64/bin/qmake)

# 设置pkg-config路径（如果需要）
# set(PKG_CONFIG_EXECUTABLE ${TOOLCHAIN_PREFIX}-pkg-config)

message(STATUS "Cross-compiling for aarch64-linux-gnu (RK3566)")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")

