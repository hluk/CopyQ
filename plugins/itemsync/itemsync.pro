include(../plugins_common.pri)

HEADERS += itemsync.h \
    ../../src/gui/iconselectbutton.h \
    ../../src/gui/iconwidget.h
SOURCES += itemsync.cpp
SOURCES += \
    ../../src/common/common.cpp \
    ../../src/gui/iconselectbutton.cpp \
    ../../src/gui/iconwidget.cpp \
    ../../src/item/serialize.cpp
FORMS   += itemsyncsettings.ui
TARGET   = $$qtLibraryTarget(itemsync)

