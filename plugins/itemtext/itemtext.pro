TEMPLATE     = lib
CONFIG      += plugin
INCLUDEPATH += ../../src/include
HEADERS      = itemtext.h \
               ../../src/include/itemwidget.h
SOURCES      = itemtext.cpp \
               ../../src/itemwidget.cpp
TARGET       = $$qtLibraryTarget(itemtext)
DESTDIR      = ../

