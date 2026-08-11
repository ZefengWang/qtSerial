cmake_minimum_required(VERSION 3.16)
project(SerialDebug VERSION 2.0.0 LANGUAGES CXX)

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
    uart_interaction.h
    uart_setting.h
    uart_core.h
    uart_interface.ui
    uart_setting.ui
    res.qrc
)

if(WIN32)
    add_executable(SerialDebug WIN32 ${PROJECT_SOURCES} logo.rc)
else()
    add_executable(SerialDebug ${PROJECT_SOURCES})
endif()

target_link_libraries(SerialDebug PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SerialPort
)

target_compile_definitions(SerialDebug PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

# Install rules
include(GNUInstallDirs)
install(TARGETS SerialDebug
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Qt deployment on Windows
if(WIN32)
    add_custom_command(TARGET SerialDebug POST_BUILD
        COMMAND Qt6::windeployqt
        ARGS --no-translations --no-system-d3d-compiler --no-opengl-sw $<TARGET_FILE:SerialDebug>
        COMMENT "Deploying Qt dependencies..."
    )
endif()
