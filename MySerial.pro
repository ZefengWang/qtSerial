#-------------------------------------------------
#
# Project: Serial Debug Assistant (Modern UI)
# Version: 2.0
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 三层架构依赖 C++17（generic lambda、make_unique 等）。
# Qt5 与 Qt6 统一使用 C++17；现代 gcc/clang 均支持。
CONFIG += c++17
# Executable name: lowercase (release/install name must match .desktop Exec & StartupWMClass)
TARGET = serial-debug
TEMPLATE = app

# Version definition.
# CI passes the version from the git tag via APP_VERSION_EXTERNAL (e.g.
#   qmake MySerial.pro APP_VERSION_EXTERNAL=2.1.0
# ). For local builds a default is kept so versioning never breaks.
isEmpty(APP_VERSION_EXTERNAL) {
    APP_VERSION_EXTERNAL = 2.1.0
}
DEFINES += APP_VERSION=\\\"$${APP_VERSION_EXTERNAL}\\\"

# 三层架构源码（core/service/plugin 纯 C++，io/ui 依赖 Qt）
# 与 CMakeLists.txt 保持一致，保证 qmake 打包管线也构建新架构。
INCLUDEPATH += $$PWD/src

SOURCES += \
    main.cpp \
    user_interaction.cpp \
    uart_setting.cpp \
    thememanager.cpp \
    languagemanager.cpp \
    src/ui/SerialWorker.cpp \
    src/io/SerialSource.cpp \
    src/io/PortMonitor.cpp \
    src/service/EventBus.cpp \
    src/service/Session.cpp \
    src/service/ProtocolEngine.cpp \
    src/service/FieldPool.cpp \
    src/service/ViewManager.cpp \
    src/core/IClock.cpp \
    src/core/FakeSource.cpp \
    src/core/buffer/RingBuffer.cpp \
    src/core/buffer/AppendBuffer.cpp \
    src/core/buffer/DoubleBuffer.cpp \
    src/protocol/LineProtocol.cpp \
    src/protocol/CsvProtocol.cpp \
    src/protocol/GenericBinaryProtocol.cpp \
    src/protocol/ProtocolRegistry.cpp \
    src/plugin/PluginRegistry.cpp

HEADERS  += \
    uart_setting.h \
    uart_interaction.h \
    thememanager.h \
    languagemanager.h \
    src/ui/SerialWorker.hpp \
    src/io/SerialSource.hpp \
    src/io/PortMonitor.hpp \
    src/service/EventBus.hpp \
    src/service/Session.hpp \
    src/service/ProtocolEngine.hpp \
    src/service/FieldPool.hpp \
    src/service/ViewManager.hpp \
    src/core/DataSource.hpp \
    src/core/IClock.hpp \
    src/core/PortConfig.hpp \
    src/core/Frame.hpp \
    src/core/FieldSchema.hpp \
    src/core/FakeSource.hpp \
    src/core/buffer/IBufferStrategy.hpp \
    src/core/buffer/RingBuffer.hpp \
    src/core/buffer/AppendBuffer.hpp \
    src/core/buffer/DoubleBuffer.hpp \
    src/protocol/IProtocol.hpp \
    src/protocol/LineProtocol.hpp \
    src/protocol/CsvProtocol.hpp \
    src/protocol/GenericBinaryProtocol.hpp \
    src/protocol/ProtocolRegistry.hpp \
    src/plugin/IPlugin.hpp \
    src/plugin/IViewHost.hpp \
    src/plugin/PluginRegistry.hpp

FORMS    += \
    uart_interface.ui \
    uart_setting.ui

TRANSLATIONS += \
    MySerial_zh_CN.ts

DISTFILES += \
    MySerial_zh_CN.qm \
    styles/dark.qss \
    styles/light.qss

RC_FILE = logo.rc

# Linux: set RPATH so the binary finds bundled Qt libs in ./lib next to itself.
# --disable-new-dtags forces DT_RPATH (searched by dlopen'd Qt plugins too,
# unlike DT_RUNPATH which only covers direct dependencies).
unix:!macx {
    QMAKE_LFLAGS += '-Wl,-rpath,\'\$$ORIGIN/lib\''
    QMAKE_LFLAGS += '-Wl,--disable-new-dtags'
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc
