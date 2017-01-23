include("../common.pri")

TEMPLATE     = lib
CONFIG      += plugin no_lflags_merge
INCLUDEPATH += ../../src
HEADERS     += ../../src/item/itemwidget.h
SOURCES     += ../../src/item/itemwidget.cpp
DESTDIR      = ../

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG(debug, debug|release) {
    DEFINES += HAS_TESTS COPYQ_DEBUG
    QT += testlib
}

# Plugins can contain undefined symbols.
QMAKE_LFLAGS_PLUGIN -= $$QMAKE_LFLAGS_NOUNDEF
