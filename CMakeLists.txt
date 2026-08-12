cmake_minimum_required(VERSION 3.16)

# Version is normally injected by CI from the git tag via -DAPP_VERSION=2.1.0.
# Keep a default so local CMake builds still work.
if(NOT DEFINED APP_VERSION)
    set(APP_VERSION "2.1.0")
endif()
project(serial-debug VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort Network)

# ============================================================
# 核心层（src/core）: 纯 C++，不依赖 Qt。
# 这是 P0 架构的可测试性基础——核心逻辑可脱离 UI 单独编译与测试。
# ============================================================
add_library(sd_core STATIC
    src/core/IClock.cpp
    src/core/FakeSource.cpp
    src/core/buffer/RingBuffer.cpp
    src/core/buffer/AppendBuffer.cpp
    src/core/buffer/DoubleBuffer.cpp
)
target_include_directories(sd_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(sd_core PUBLIC cxx_std_17)

# ============================================================
# Service 层（src/service）: 依赖核心层，纯 C++。
# ============================================================
add_library(sd_service STATIC
    src/service/EventBus.cpp
    src/service/Session.cpp
    src/service/ProtocolEngine.cpp
    src/service/FieldPool.cpp
    src/service/ViewManager.cpp
)
target_include_directories(sd_service PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sd_service PUBLIC sd_core sd_protocol)
target_compile_features(sd_service PUBLIC cxx_std_17)

# ============================================================
# 协议层（src/protocol）: 依赖核心层，纯 C++。
# 自定义协议扩展点；波形/3D 等均建立在协议解析之上。
# ============================================================
add_library(sd_protocol STATIC
    src/protocol/LineProtocol.cpp
    src/protocol/CsvProtocol.cpp
    src/protocol/GenericBinaryProtocol.cpp
    src/protocol/ProtocolRegistry.cpp
)
target_include_directories(sd_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sd_protocol PUBLIC sd_core)
target_compile_features(sd_protocol PUBLIC cxx_std_17)

# ============================================================
# 插件层（src/plugin）: 依赖核心层接口，纯 C++。
# ============================================================
add_library(sd_plugin STATIC
    src/plugin/PluginRegistry.cpp
)
target_include_directories(sd_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sd_plugin PUBLIC sd_core)
target_compile_features(sd_plugin PUBLIC cxx_std_17)

# ============================================================
# IO 层（src/io）: 生产环境数据源，依赖 Qt SerialPort。
# 把 QSerialPort 封装到 DataSource 接口背后，供 Service/UI 使用。
# ============================================================
add_library(sd_io STATIC
    src/io/SerialSource.cpp
    src/io/PortMonitor.cpp
)
target_include_directories(sd_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sd_io PUBLIC sd_core Qt6::SerialPort)
target_compile_features(sd_io PUBLIC cxx_std_17)

set(PROJECT_SOURCES
    main.cpp
    user_interaction.cpp
    uart_setting.cpp
    thememanager.cpp
    languagemanager.cpp
    uart_interaction.h
    uart_setting.h
    thememanager.h
    languagemanager.h
    uart_interface.ui
    uart_setting.ui
    res.qrc
)

# ============================================================
# UI 适配层（src/ui）: SerialWorker 封装核心到 Qt 信号。
# 抽成独立库，供各宿主复用。仅依赖 Qt Core（无 Widgets）。
# ============================================================
add_library(sd_ui STATIC
    src/ui/SerialWorker.cpp
    src/ui/SerialWorker.hpp
)
target_include_directories(sd_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sd_ui PUBLIC sd_core sd_service sd_io Qt6::Core Qt6::SerialPort)
target_compile_features(sd_ui PUBLIC cxx_std_17)

# ============================================================
# Launcher 层（src/launcher）: 统一入口的 UI 模式管理 + 各宿主运行逻辑。
# 有图形环境时复用 QApplication；无图形用 QCoreApplication。
# ============================================================
find_package(Qt6 QUIET COMPONENTS Quick Qml)
add_library(sd_launcher STATIC
    src/launcher/UiMode.cpp
    src/launcher/UiMode.hpp
    src/launcher/UiModePicker.cpp
    src/launcher/UiModePicker.hpp
    src/launcher/UiRunners.hpp
    src/launcher/run_tui.cpp
    src/launcher/run_web.cpp
    src/launcher/run_qml.cpp
)
target_include_directories(sd_launcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sd_launcher PUBLIC sd_ui Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
target_compile_features(sd_launcher PUBLIC cxx_std_17)

# 有 Qt Quick/Qml 时启用 QML 宿主（HAVE_QML），并链接 QML 资源。
if(Qt6Quick_FOUND AND Qt6Qml_FOUND)
    target_compile_definitions(sd_launcher PRIVATE HAVE_QML)
    target_sources(sd_launcher PRIVATE src/qml/qml_resources.qrc)
    target_link_libraries(sd_launcher PUBLIC Qt6::Quick Qt6::Qml)
endif()

if(WIN32)
    add_executable(serial-debug WIN32 ${PROJECT_SOURCES} logo.rc)
else()
    add_executable(serial-debug ${PROJECT_SOURCES})
endif()

# Compile .ts -> .qm using lrelease (Qt Linguist tool)
# 优先使用 Qt6 自带的 lrelease（Debian 位于 /usr/lib/qt6/bin），
# 避免误用 PATH 中损坏的 /usr/bin/lrelease 包装器。
find_program(QT_LRELEASE_EXECUTABLE
    NAMES lrelease-qt6 lrelease6 lrelease
    HINTS ${Qt6_DIR}/bin /usr/lib/qt6/bin /usr/lib64/qt6/bin
)
if(QT_LRELEASE_EXECUTABLE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/MySerial_zh_CN.qm
        COMMAND ${QT_LRELEASE_EXECUTABLE}
                -removeidentical
                ARGS ${CMAKE_CURRENT_SOURCE_DIR}/MySerial_zh_CN.ts
                     -qm ${CMAKE_CURRENT_BINARY_DIR}/MySerial_zh_CN.qm
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/MySerial_zh_CN.ts
        COMMENT "Running lrelease to generate MySerial_zh_CN.qm"
    )
    add_custom_target(translations ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/MySerial_zh_CN.qm
    )
endif()

target_link_libraries(serial-debug PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Network
    sd_core
    sd_service
    sd_plugin
    sd_io
    sd_ui
    sd_launcher
)

target_compile_definitions(serial-debug PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

# Install rules
include(GNUInstallDirs)
install(TARGETS serial-debug
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Qt deployment on Windows
if(WIN32)
    add_custom_command(TARGET serial-debug POST_BUILD
        COMMAND Qt6::windeployqt
        ARGS --no-translations --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:serial-debug>
        COMMENT "Deploying Qt dependencies..."
    )
endif()

# 核心层单元测试（纯 C++，无 Qt 依赖）
enable_testing()
add_subdirectory(tests)
