TEMPLATE     = lib
CONFIG      += plugin
INCLUDEPATH += ../../src/include
HEADERS      = itemdata.h \
               ../../src/include/itemwidget.h
SOURCES      = itemdata.cpp \
               ../../src/itemwidget.cpp
TARGET       = $$qtLibraryTarget(itemdata)
DESTDIR      = ../

