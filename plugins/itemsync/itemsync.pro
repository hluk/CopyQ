include(../plugins_common.pri)

HEADERS += itemsync.h
SOURCES += itemsync.cpp
SOURCES += \
    ../../src/common/common.cpp \
    ../../src/gui/iconselectbutton.cpp \
    ../../src/gui/iconwidget.cpp
FORMS   += itemsyncsettings.ui
TARGET   = $$qtLibraryTarget(itemsync)

