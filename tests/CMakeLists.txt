# 核心层测试：纯 C++，不依赖 Qt，可在任何 g++ 环境编译运行。
# 这是 P0 可测试性内建的验证——核心逻辑脱离 UI/硬件即可测试。
#
# 测试金字塔（见重构计划 CH.07）：
#   - 单元测试：buffer / session / plugin（纯 C++，无 Qt）
#   - 集成测试：integration（数据通路全链路，纯 C++，无 Qt）
#   - 系统测试：gui_system_test（Qt Test，offscreen 启动真实 GUI）

# 核心层 + Service 层 + 插件层源码（纯 C++，无 Qt 依赖）
set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/IClock.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FakeSource.cpp
    ${CMAKE_SOURCE_DIR}/src/core/buffer/RingBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/buffer/AppendBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/buffer/DoubleBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/service/EventBus.cpp
    ${CMAKE_SOURCE_DIR}/src/service/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/service/ProtocolEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/service/FieldPool.cpp
    ${CMAKE_SOURCE_DIR}/src/service/ViewManager.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/PluginRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/LineProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/CsvProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/GenericBinaryProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/protocol/ProtocolRegistry.cpp
)

# 计算需要覆盖的行覆盖率的源文件（gcov）
set(COVERAGE_SOURCES ${CORE_SOURCES})

# ---------- 单元测试 + 集成测试：纯 C++ ----------
add_executable(core_buffer_test buffer_test.cpp ${CORE_SOURCES})
add_executable(core_session_test session_test.cpp ${CORE_SOURCES})
add_executable(core_plugin_test plugin_test.cpp ${CORE_SOURCES})
add_executable(core_integration_test integration_test.cpp ${CORE_SOURCES})
add_executable(core_protocol_test protocol_test.cpp ${CORE_SOURCES})
add_executable(core_schema_test schema_test.cpp ${CORE_SOURCES})

# 测试需要 C++17
foreach(t core_buffer_test core_session_test core_plugin_test core_integration_test core_protocol_test core_schema_test)
    target_compile_features(${t} PRIVATE cxx_std_17)
    target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR}/src)
endforeach()

enable_testing()
add_test(NAME buffer_test COMMAND core_buffer_test)
add_test(NAME session_test COMMAND core_session_test)
add_test(NAME plugin_test COMMAND core_plugin_test)
add_test(NAME integration_test COMMAND core_integration_test)
add_test(NAME protocol_test COMMAND core_protocol_test)
add_test(NAME schema_test COMMAND core_schema_test)

# ---------- 系统测试：Qt Test + offscreen GUI ----------
# 复用主工程的核心实现，验证真实 GUI 装配（God Object 拆解后仍可构建）。
find_package(Qt6 COMPONENTS Test Gui Widgets)
if(Qt6_FOUND)
    add_executable(gui_system_test gui_system_test.cpp
        ${CMAKE_SOURCE_DIR}/user_interaction.cpp
        ${CMAKE_SOURCE_DIR}/uart_setting.cpp
        ${CMAKE_SOURCE_DIR}/thememanager.cpp
        ${CMAKE_SOURCE_DIR}/languagemanager.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/SerialWorker.cpp
        # UI 定义交由 CMAKE_AUTOUIC 生成
        ${CMAKE_SOURCE_DIR}/uart_interaction.h
        ${CMAKE_SOURCE_DIR}/uart_setting.h
        ${CMAKE_SOURCE_DIR}/uart_interface.ui
        ${CMAKE_SOURCE_DIR}/uart_setting.ui
        ${CMAKE_SOURCE_DIR}/res.qrc
    )
    target_include_directories(gui_system_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(gui_system_test PRIVATE
        Qt6::Test Qt6::Gui Qt6::Widgets Qt6::SerialPort Qt6::Network
        sd_core sd_service sd_io sd_ui sd_launcher
    )
    target_compile_features(gui_system_test PRIVATE cxx_std_17)
    add_test(NAME gui_system_test COMMAND gui_system_test)
    # offscreen 平台运行，无需 X display
    set_tests_properties(gui_system_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    # PortMonitor 单元测试（Qt Test，注入假扫描器，无需硬件）
    add_executable(port_monitor_test port_monitor_test.cpp
        ${CMAKE_SOURCE_DIR}/src/io/PortMonitor.cpp
    )
    target_include_directories(port_monitor_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(port_monitor_test PRIVATE Qt6::Test Qt6::Core)
    target_compile_features(port_monitor_test PRIVATE cxx_std_17)
    add_test(NAME port_monitor_test COMMAND port_monitor_test)
    set_tests_properties(port_monitor_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()

# ---------- 覆盖率（可选，需 gcov） ----------
option(ENABLE_COVERAGE "Enable gcov coverage" OFF)
if(ENABLE_COVERAGE)
    find_program(GCOV_EXECUTABLE gcov)
    if(GCOV_EXECUTABLE)
        foreach(t core_buffer_test core_session_test core_plugin_test core_integration_test)
            target_compile_options(${t} PRIVATE --coverage -g -O0)
            target_link_options(${t} PRIVATE --coverage)
        endforeach()
    endif()
endif()