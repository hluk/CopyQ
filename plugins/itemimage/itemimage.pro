include(../plugins_common.pri)

HEADERS += itemimage.h
SOURCES += itemimage.cpp
SOURCES += ../../src/common/common.cpp
FORMS   += itemimagesettings.ui
TARGET   = $$qtLibraryTarget(itemimage)

HEADERS += ../../src/item/itemeditor.h
SOURCES += ../../src/item/itemeditor.cpp

