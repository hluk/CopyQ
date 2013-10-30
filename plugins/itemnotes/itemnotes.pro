include(../plugins_common.pri)

HEADERS += itemnotes.h
SOURCES += itemnotes.cpp \
    ../../src/gui/iconwidget.cpp
FORMS   += itemnotessettings.ui
TARGET   = $$qtLibraryTarget(itemnotes)

