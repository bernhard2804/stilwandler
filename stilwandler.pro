QT += core gui qml quick quickcontrols2 network

CONFIG += c++17 release
TARGET = stilwandler
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/styles.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/styles.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
