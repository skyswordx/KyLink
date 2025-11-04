cmake_minimum_required(VERSION 3.16)
project(FeiQChatroom VERSION 1.0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt自动处理元对象系统
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

# 查找Qt5 (要求5.12或更高版本，兼容5.12.8)
find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Threads REQUIRED)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/feiqlib
)

# UI文件
file(GLOB UI_FILES "${CMAKE_SOURCE_DIR}/ui/*.ui")

# 源文件
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/FeiqBackend.cpp
    src/ChatWindow.cpp
    src/SettingsDialog.cpp
    src/AboutDialog.cpp
    src/GroupChatDialog.cpp
    src/ScreenshotTool.cpp
)

set(HEADERS
    include/MainWindow.h
    include/ChatWindow.h
    include/FeiqBackend.h
    include/FeiqTypes.h
    include/SettingsDialog.h
    include/AboutDialog.h
    include/GroupChatDialog.h
    include/ScreenshotTool.h
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
    feiqlib
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE PROJECT_VERSION_STR="${PROJECT_VERSION}"
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

