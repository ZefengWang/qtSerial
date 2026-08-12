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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SerialPort)

set(PROJECT_SOURCES
    main.cpp
    user_interaction.cpp
    uart_setting.cpp
    uart_core.cpp
    thememanager.cpp
    languagemanager.cpp
    uart_interaction.h
    uart_setting.h
    uart_core.h
    thememanager.h
    languagemanager.h
    uart_interface.ui
    uart_setting.ui
    res.qrc
)

if(WIN32)
    add_executable(serial-debug WIN32 ${PROJECT_SOURCES} logo.rc)
else()
    add_executable(serial-debug ${PROJECT_SOURCES})
endif()

# Compile .ts -> .qm using lrelease (Qt Linguist tool)
find_program(QT_LRELEASE_EXECUTABLE
    NAMES lrelease lrelease-qt6 lrelease6
    HINTS ${Qt6_DIR}/bin
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
