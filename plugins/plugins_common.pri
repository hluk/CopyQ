TEMPLATE     = lib
CONFIG      += plugin
INCLUDEPATH += ../../src/include
HEADERS     += ../../src/include/itemwidget.h
SOURCES     += ../../src/itemwidget.cpp
DESTDIR      = ../

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

