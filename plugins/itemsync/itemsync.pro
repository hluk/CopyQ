include(../plugins_common.pri)

HEADERS += itemsync.h
SOURCES += itemsync.cpp
FORMS   += itemsyncsettings.ui

CONFIG(debug, debug|release) {
    SOURCES += tests/itemsynctests.cpp
    HEADERS += tests/itemsynctests.h
}

TARGET = itemsync

