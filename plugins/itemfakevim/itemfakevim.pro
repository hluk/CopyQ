include(../plugins_common.pri)

HEADERS += itemfakevim.h
SOURCES += itemfakevim.cpp \
    ../../src/item/itemwidgetwrapper.cpp
FORMS   += itemfakevimsettings.ui
TARGET   = $$qtLibraryTarget(itemfakevim)
RESOURCES += itemfakevim.qrc
DEFINES += QTCREATOR_UTILS_STATIC_LIB

CONFIG(debug, debug|release) {
    SOURCES += tests/itemfakevimtests.cpp
    HEADERS += tests/itemfakevimtests.h
}

include(fakevim/fakevim.pri)

# Omit translating texts for FakeVim.
lupdate_only {
    SOURCES = itemfakevim.cpp
}

