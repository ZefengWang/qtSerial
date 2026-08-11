#-------------------------------------------------
#
# Project: Serial Debug Assistant (Modern UI)
# Version: 2.0
#
#-------------------------------------------------

QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
TARGET = SerialDebug
TEMPLATE = app

# Version definition
DEFINES += APP_VERSION=\\\"2.0\\\"

SOURCES += \
    main.cpp \
    user_interaction.cpp \
    uart_setting.cpp \
    uart_core.cpp

HEADERS  += \
    uart_core.h \
    uart_setting.h \
    uart_interaction.h

FORMS    += \
    uart_interface.ui \
    uart_setting.ui

TRANSLATIONS += \
    MySerial_zh_CN.ts

DISTFILES += \
    MySerial_zh_CN.qm \
    styles/dark.qss

RC_FILE = logo.rc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc
