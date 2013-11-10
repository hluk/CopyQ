include(../plugins_common.pri)

HEADERS += itemnotes.h \
    ../../src/gui/iconwidget.h
SOURCES += itemnotes.cpp \
    ../../src/gui/iconfont.cpp \
    ../../src/gui/iconwidget.cpp
FORMS   += itemnotessettings.ui
TARGET   = $$qtLibraryTarget(itemnotes)

