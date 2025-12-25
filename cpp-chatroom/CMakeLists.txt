cmake_minimum_required(VERSION 3.16)
project(FeiQChatroom VERSION 1.1.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# 平台选项
# =============================================================================
option(BUILD_RK3566 "为 RK3566 平台构建 (启用 NPU/RGA/MPP)" OFF)
option(BUILD_DESKTOP "为桌面平台构建 (无硬件加速依赖)" OFF)

# 自动检测平台
if(NOT BUILD_RK3566 AND NOT BUILD_DESKTOP)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_CROSSCOMPILING)
        set(BUILD_RK3566 ON)
        message(STATUS "自动检测: 启用 RK3566 构建模式")
    else()
        set(BUILD_DESKTOP ON)
        message(STATUS "自动检测: 启用桌面构建模式")
    endif()
endif()

message(STATUS "========================================")
message(STATUS "构建配置:")
message(STATUS "  BUILD_RK3566  = ${BUILD_RK3566}")
message(STATUS "  BUILD_DESKTOP = ${BUILD_DESKTOP}")
message(STATUS "========================================")

# =============================================================================
# Qt 自动处理
# =============================================================================
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

# =============================================================================
# 依赖查找
# =============================================================================
find_package(Qt5 5.12.8 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# 可选: Qt Multimedia (用于摄像头)
find_package(Qt5 COMPONENTS Multimedia MultimediaWidgets QUIET)
if(Qt5Multimedia_FOUND)
    set(HAVE_QT_MULTIMEDIA ON)
    message(STATUS "找到 Qt5::Multimedia")
else()
    set(HAVE_QT_MULTIMEDIA OFF)
    message(STATUS "未找到 Qt5::Multimedia, 摄像头功能禁用")
endif()

# RK3566 特有依赖
if(BUILD_RK3566)
    # GStreamer
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-video-1.0 gstreamer-app-1.0 gstreamer-allocators-1.0)
    
    # RGA
    pkg_check_modules(RGA REQUIRED librga)
    
    # Rockchip MPP
    pkg_check_modules(RKMPP REQUIRED rockchip_mpp)
    
    # OpenCV
    find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
    
    # RKNN
    set(_rknn_hint_paths)
    if(CMAKE_SYSROOT)
        list(APPEND _rknn_hint_paths
            ${CMAKE_SYSROOT}/usr/include
            ${CMAKE_SYSROOT}/usr/lib
            ${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu)
    endif()
    list(APPEND _rknn_hint_paths
        /usr/include
        /usr/lib
        /usr/lib/aarch64-linux-gnu)

    find_path(RKNN_INCLUDE_DIR rknn_api.h
        HINTS ${_rknn_hint_paths}
        PATH_SUFFIXES rknn)
    find_library(RKNNRT_LIBRARY NAMES rknnrt
        HINTS ${_rknn_hint_paths}
        PATH_SUFFIXES lib)

    if(NOT RKNN_INCLUDE_DIR OR NOT RKNNRT_LIBRARY)
        message(FATAL_ERROR "RK3566 模式下需要 RKNN 库，但未找到。")
    endif()
    
    message(STATUS "RK3566 依赖检查通过")
endif()

# =============================================================================
# 输出目录
# =============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# =============================================================================
# 包含目录
# =============================================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/feiqlib
)

# =============================================================================
# 源文件
# =============================================================================
# 通用源文件
set(COMMON_SOURCES
    src/app/main.cpp
    src/backend/FeiqBackend.cpp
    src/ui/AboutDialog.cpp
    src/ui/ChatWindow.cpp
    src/ui/GroupChatDialog.cpp
    src/ui/MainWindow.cpp
    src/ui/SettingsDialog.cpp
)

set(COMMON_HEADERS
    include/backend/FeiqBackend.h
    include/domain/FeiqTypes.h
    include/ui/AboutDialog.h
    include/ui/ChatWindow.h
    include/ui/GroupChatDialog.h
    include/ui/MainWindow.h
    include/ui/SettingsDialog.h
)

# RK3566 特有源文件
set(RK3566_SOURCES
    src/backend/RgaSelfTest.cpp
    src/backend/PerformanceMonitor.cpp
    src/npu/YoloV5Runner.cpp
    src/npu/RgaPreprocessor.cpp
    src/npu/postprocess.cpp
    src/ui/CameraPreviewDialog.cpp
    src/ui/PerformanceAnalyticsDialog.cpp
    src/ui/ScreenshotTool.cpp
)

set(RK3566_HEADERS
    include/backend/RgaSelfTest.h
    include/backend/PerformanceMonitor.h
    include/npu/YoloV5Runner.h
    include/npu/RgaPreprocessor.h
    include/npu/postprocess.h
    include/ui/CameraPreviewDialog.h
    include/ui/PerformanceAnalyticsDialog.h
    include/ui/ScreenshotTool.h
)

# 根据平台选择源文件
if(BUILD_RK3566)
    set(SOURCES ${COMMON_SOURCES} ${RK3566_SOURCES})
    set(HEADERS ${COMMON_HEADERS} ${RK3566_HEADERS})
else()
    set(SOURCES ${COMMON_SOURCES})
    set(HEADERS ${COMMON_HEADERS})
endif()

# UI 文件
file(GLOB UI_FILES "${CMAKE_SOURCE_DIR}/ui/*.ui")

# =============================================================================
# feiqlib 静态库
# =============================================================================
file(GLOB FEIQLIB_SOURCES "${CMAKE_SOURCE_DIR}/feiqlib/*.cpp")
add_library(feiqlib STATIC ${FEIQLIB_SOURCES})
target_include_directories(feiqlib PUBLIC ${CMAKE_SOURCE_DIR}/feiqlib)
target_link_libraries(feiqlib PUBLIC Qt5::Core Threads::Threads)

find_package(Iconv)
if(Iconv_FOUND)
    target_link_libraries(feiqlib PUBLIC Iconv::Iconv)
endif()

# =============================================================================
# 主可执行文件
# =============================================================================
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${UI_FILES})

# 基础链接
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    feiqlib
)

# 编译定义
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PROJECT_VERSION_STR="${PROJECT_VERSION}"
)

if(BUILD_RK3566)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        BUILD_RK3566=1
        HAVE_RGA=1
    )
endif()

if(BUILD_DESKTOP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        BUILD_DESKTOP=1
    )
endif()

# =============================================================================
# 平台特定链接
# =============================================================================
if(BUILD_RK3566)
    # Qt Multimedia
    if(HAVE_QT_MULTIMEDIA)
        target_link_libraries(${PROJECT_NAME} Qt5::Multimedia Qt5::MultimediaWidgets)
    endif()
    
    # GStreamer
    target_include_directories(${PROJECT_NAME} PRIVATE ${GSTREAMER_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${GSTREAMER_LIBRARY_DIRS})
    target_compile_options(${PROJECT_NAME} PRIVATE ${GSTREAMER_CFLAGS_OTHER})
    target_link_libraries(${PROJECT_NAME} ${GSTREAMER_LIBRARIES})
    
    # OpenCV
    target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})
    
    # RKNN
    target_include_directories(${PROJECT_NAME} PRIVATE ${RKNN_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${RKNNRT_LIBRARY})
    
    # RGA
    target_include_directories(${PROJECT_NAME} PRIVATE ${RGA_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${RGA_LIBRARY_DIRS})
    target_compile_options(${PROJECT_NAME} PRIVATE ${RGA_CFLAGS_OTHER})
    target_link_libraries(${PROJECT_NAME} ${RGA_LIBRARIES})
    
    # MPP
    target_include_directories(${PROJECT_NAME} PRIVATE ${RKMPP_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${RKMPP_LIBRARY_DIRS})
    target_compile_options(${PROJECT_NAME} PRIVATE ${RKMPP_CFLAGS_OTHER})
    target_link_libraries(${PROJECT_NAME} ${RKMPP_LIBRARIES})
    
    # 复制 NPU 模型资源
    set(YOLO_REFERENCE_ROOT ${CMAKE_SOURCE_DIR}/../Reference/YoloV5-NPU)
    set(NPU_MODEL_SRC ${YOLO_REFERENCE_ROOT}/rk3566/yolov5s_relu.rknn)
    set(NPU_IMAGE_SRC ${YOLO_REFERENCE_ROOT}/busstop.jpg)
    
    if(EXISTS ${NPU_MODEL_SRC})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/npu_assets
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NPU_MODEL_SRC} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/npu_assets/
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NPU_IMAGE_SRC} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/npu_assets/
            COMMENT "复制 NPU 示例模型和图片到运行目录"
        )
    endif()
endif()

# =============================================================================
# Windows 特定设置
# =============================================================================
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE FALSE)
endif()

# =============================================================================
# 工具链文件提示
# =============================================================================
if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "使用工具链文件: ${CMAKE_TOOLCHAIN_FILE}")
endif()

message(STATUS "========================================")
message(STATUS "配置完成，准备构建 ${PROJECT_NAME}")
message(STATUS "========================================")
