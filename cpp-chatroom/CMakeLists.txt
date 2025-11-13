cmake_minimum_required(VERSION 3.16)
project(FeiQChatroom VERSION 1.1.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt自动处理元对象系统
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

# 查找Qt5 (要求 5.12.8 或更高版本，当前在 Ubuntu 上链接 5.12.8)
find_package(Qt5 5.12.8 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia MultimediaWidgets)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-video-1.0)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

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

if(NOT RKNN_INCLUDE_DIR)
    message(FATAL_ERROR "未找到 rknn_api.h，请设置 RKNN_INCLUDE_DIR 或确认 sysroot 内容。")
endif()

if(NOT RKNNRT_LIBRARY)
    message(FATAL_ERROR "未找到 rknnrt 库，请设置 RKNNRT_LIBRARY 或确认 sysroot 内容。")
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/feiqlib
)

# UI文件
file(GLOB UI_FILES "${CMAKE_SOURCE_DIR}/ui/*.ui")

# 源文件
set(SOURCES
    src/app/main.cpp
    src/backend/FeiqBackend.cpp
    src/npu/YoloV5Runner.cpp
    src/npu/postprocess.cpp
    src/ui/AboutDialog.cpp
    src/ui/ChatWindow.cpp
    src/ui/CameraPreviewDialog.cpp
    src/ui/GroupChatDialog.cpp
    src/ui/MainWindow.cpp
    src/ui/ScreenshotTool.cpp
    src/ui/SettingsDialog.cpp
)

set(HEADERS
    include/backend/FeiqBackend.h
    include/domain/FeiqTypes.h
    include/npu/YoloV5Runner.h
    include/npu/postprocess.h
    include/ui/AboutDialog.h
    include/ui/ChatWindow.h
    include/ui/CameraPreviewDialog.h
    include/ui/GroupChatDialog.h
    include/ui/MainWindow.h
    include/ui/ScreenshotTool.h
    include/ui/SettingsDialog.h
)

file(GLOB FEIQLIB_SOURCES "${CMAKE_SOURCE_DIR}/feiqlib/*.cpp")

add_library(feiqlib STATIC ${FEIQLIB_SOURCES})
target_include_directories(feiqlib PUBLIC ${CMAKE_SOURCE_DIR}/feiqlib)
target_link_libraries(feiqlib PUBLIC Qt5::Core Threads::Threads)

find_package(Iconv)
if(Iconv_FOUND)
    target_link_libraries(feiqlib PUBLIC Iconv::Iconv)
endif()

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${UI_FILES})

# 链接Qt库
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    feiqlib
    ${GSTREAMER_LIBRARIES}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${GSTREAMER_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${RKNN_INCLUDE_DIR})
target_link_directories(${PROJECT_NAME} PRIVATE ${GSTREAMER_LIBRARY_DIRS})
target_compile_options(${PROJECT_NAME} PRIVATE ${GSTREAMER_CFLAGS_OTHER})

target_compile_definitions(${PROJECT_NAME}
    PRIVATE PROJECT_VERSION_STR="${PROJECT_VERSION}"
)

target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
    ${RKNNRT_LIBRARY}
)

set(YOLO_REFERENCE_ROOT ${CMAKE_SOURCE_DIR}/../Reference/YoloV5-NPU)
set(NPU_MODEL_SRC ${YOLO_REFERENCE_ROOT}/rk3566/yolov5s_relu.rknn)
set(NPU_IMAGE_SRC ${YOLO_REFERENCE_ROOT}/busstop.jpg)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/npu_assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NPU_MODEL_SRC} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/npu_assets/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NPU_IMAGE_SRC} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/npu_assets/
    BYPRODUCTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/npu_assets/yolov5s_relu.rknn
               ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/npu_assets/busstop.jpg
    COMMENT "复制 NPU 示例模型和图片到运行目录"
)

# Windows特定设置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
endif()

# 交叉编译支持
# 使用方式: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake ..
if(CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()

