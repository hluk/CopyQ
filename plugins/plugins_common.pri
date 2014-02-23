TEMPLATE     = lib
CONFIG      += plugin
INCLUDEPATH += ../../src
HEADERS     += ../../src/item/itemwidget.h
SOURCES     += ../../src/item/itemwidget.cpp
DESTDIR      = ../

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG(debug, debug|release) {
    DEFINES += HAS_TESTS COPYQ_LOG_DEBUG
    QT += testlib
}

