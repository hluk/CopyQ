include(../plugins_common.pri)

HEADERS += itemsync.h
SOURCES += itemsync.cpp
SOURCES += ../../src/common/common.cpp
FORMS   += itemsyncsettings.ui
TARGET   = $$qtLibraryTarget(itemsync)

