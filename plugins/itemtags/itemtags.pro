include(../plugins_common.pri)

HEADERS += itemtags.h
SOURCES += itemtags.cpp
FORMS   += itemtagssettings.ui

CONFIG(debug, debug|release) {
    SOURCES += tests/itemtagstests.cpp
    HEADERS += tests/itemtagstests.h
}

TARGET   = itemtags

