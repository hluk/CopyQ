include(../plugins_common.pri)

HEADERS += itemencrypted.h
SOURCES += itemencrypted.cpp
SOURCES += ../../src/item/encrypt.cpp
FORMS   += itemencryptedsettings.ui
TARGET   = $$qtLibraryTarget(itemencrypted)

