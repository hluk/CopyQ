include(../plugins_common.pri)

HEADERS += itempinned.h
SOURCES += itempinned.cpp \
    ../../src/common/display.cpp
FORMS   += itempinnedsettings.ui
TARGET   = $$qtLibraryTarget(itempinned)

CONFIG(debug, debug|release) {
    SOURCES += tests/itempinnedtests.cpp
    HEADERS += tests/itempinnedtests.h
}

