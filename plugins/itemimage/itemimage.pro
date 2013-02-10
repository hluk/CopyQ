TEMPLATE     = lib
CONFIG      += plugin
INCLUDEPATH += ../../src/include
HEADERS      = itemimage.h \
               ../../src/include/itemwidget.h
SOURCES      = itemimage.cpp \
               ../../src/itemwidget.cpp
TARGET       = $$qtLibraryTarget(itemimage)
DESTDIR      = ../

