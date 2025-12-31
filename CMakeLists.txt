cmake_minimum_required(VERSION 3.10)
project(XS-VLM-OCR LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 搜索：优先 Qt6，失败再退回 Qt5；可在命令行通过 CMAKE_PREFIX_PATH 指定安装路径
find_package(Qt6 COMPONENTS Widgets Network Concurrent QUIET)
if(Qt6_FOUND)
    set(QT_PACKAGE Qt6)
else()
    find_package(Qt5 COMPONENTS Widgets Network Concurrent REQUIRED)
    set(QT_PACKAGE Qt5)
endif()

if(MINGW)
    # 先清掉之前缓存的 RC 设置，避免拿到 32 位 windres
    unset(CMAKE_RC_COMPILER CACHE)
    unset(CMAKE_RC_FLAGS CACHE)

    # 检查 Qt 路径与编译器位宽是否一致
    set(_qt_core_dir "")
    if(Qt6_FOUND)
        set(_qt_core_dir "${Qt6Core_DIR}")
    else()
        set(_qt_core_dir "${Qt5Core_DIR}")
    endif()
    get_filename_component(_tool_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(_windres_exe NAMES windres x86_64-w64-mingw32-windres i686-w64-mingw32-windres HINTS "${_tool_bin}")
    if(_windres_exe)
        set(CMAKE_RC_COMPILER "${_windres_exe}" CACHE FILEPATH "Resource compiler" FORCE)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --target=pe-x86-64")
        else()
            set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} --target=pe-i386")
        endif()
    endif()
endif()

# Windows 图标资源
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
endif()

# 指定 MSVC UTF-8 编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 收集源文件
set(CORE_SOURCES
    src/core/OCRPipeline.cpp
)

set(CORE_HEADERS
    src/core/OCRResult.h
    src/core/ModelAdapter.h
    src/core/OCRPipeline.h
)

set(ADAPTER_SOURCES
    src/adapters/TesseractAdapter.cpp
    src/adapters/QwenAdapter.cpp
    src/adapters/CustomAdapter.cpp
    src/adapters/GLMAdapter.cpp
    src/adapters/PaddleAdapter.cpp
    src/adapters/DoubaoAdapter.cpp
    src/adapters/GeneralAdapter.cpp
    src/adapters/GeminiAdapter.cpp
)

set(ADAPTER_HEADERS
    src/adapters/TesseractAdapter.h
    src/adapters/QwenAdapter.h
    src/adapters/CustomAdapter.h
    src/adapters/GLMAdapter.h
    src/adapters/PaddleAdapter.h
    src/adapters/DoubaoAdapter.h
    src/adapters/GeneralAdapter.h
    src/adapters/GeminiAdapter.h
)

set(MANAGER_SOURCES
    src/managers/ModelManager.cpp
    src/managers/ClipboardManager.cpp
)

set(MANAGER_HEADERS
    src/managers/ModelManager.h
    src/managers/ClipboardManager.h
)

set(UTILS_SOURCES
    src/utils/ConfigManager.cpp
)

set(UTILS_HEADERS
    src/utils/ConfigManager.h
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/SettingsDialog.cpp
)

set(UI_HEADERS
    src/ui/MainWindow.h
    src/ui/SettingsDialog.h
)

set(UI_RESOURCES
    src/ui/resources.qrc
)

set(MAIN_SOURCE
    src/main.cpp
)

# 所有源文件
set(ALL_SOURCES
    ${MAIN_SOURCE}
    ${CORE_SOURCES}
    ${ADAPTER_SOURCES}
    ${MANAGER_SOURCES}
    ${UTILS_SOURCES}
    ${UI_SOURCES}
    ${UI_RESOURCES}
)

set(ALL_HEADERS
    ${CORE_HEADERS}
    ${ADAPTER_HEADERS}
    ${MANAGER_HEADERS}
    ${UTILS_HEADERS}
    ${UI_HEADERS}
)

# 创建可执行文件
# 让 Qt GUI 程序不弹出控制台，需把可执行目标设为 GUI 子系统
add_executable(${PROJECT_NAME} WIN32
    ${ALL_SOURCES}
    ${ALL_HEADERS}
    $<$<PLATFORM_ID:Windows>:${APP_ICON_RESOURCE_WINDOWS}>
)

# Debug 模式，弹出控制台
# add_executable(${PROJECT_NAME}
#     ${ALL_SOURCES}
#     ${ALL_HEADERS}
#     $<$<PLATFORM_ID:Windows>:${APP_ICON_RESOURCE_WINDOWS}>
# )

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${QT_PACKAGE}::Widgets
    ${QT_PACKAGE}::Network
    ${QT_PACKAGE}::Concurrent
)

# 复制配置模板文件到构建目录
configure_file(
    ${CMAKE_SOURCE_DIR}/models_config.json
    ${CMAKE_BINARY_DIR}/models_config.json
    COPYONLY
)

message(STATUS "配置模板文件将被复制到构建目录")

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Windows 特定设置
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE user32)
endif()

# 输出信息
message(STATUS "======================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Qt Package: ${QT_PACKAGE}")
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "======================================")