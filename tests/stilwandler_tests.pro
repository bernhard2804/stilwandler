QT += core gui quick quickcontrols2 testlib network
CONFIG += c++17 testcase
TARGET = stilwandler_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/styles.h ../src/theme.h ../src/translator.h
SOURCES += stilwandler_tests.cpp ../src/backend.cpp ../src/styles.cpp ../src/theme.cpp ../src/translator.cpp
RESOURCES += ../src/resources.qrc
