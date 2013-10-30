include(../plugins_common.pri)

HEADERS += itemencrypted.h
SOURCES += itemencrypted.cpp
SOURCES += \
    ../../src/common/common.cpp \
    ../../src/gui/iconwidget.cpp \
    ../../src/item/encrypt.cpp \
    ../../src/item/serialize.cpp
FORMS   += itemencryptedsettings.ui
TARGET   = $$qtLibraryTarget(itemencrypted)

