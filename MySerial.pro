#-------------------------------------------------
#
# Project: Serial Debug Assistant (Modern UI)
# Version: 2.0
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# Use C++17 for Qt6, C++11 for Qt5 (for broader compatibility)
greaterThan(QT_MAJOR_VERSION, 5) {
    CONFIG += c++17
} else {
    CONFIG += c++11
}
# Executable name: lowercase (release/install name must match .desktop Exec & StartupWMClass)
TARGET = serial-debug
TEMPLATE = app

# Version definition
DEFINES += APP_VERSION=\\\"2.0.1\\\"

SOURCES += \
    main.cpp \
    user_interaction.cpp \
    uart_setting.cpp \
    uart_core.cpp \
    thememanager.cpp \
    languagemanager.cpp

HEADERS  += \
    uart_core.h \
    uart_setting.h \
    uart_interaction.h \
    thememanager.h \
    languagemanager.h

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
