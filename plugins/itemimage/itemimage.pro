include(../plugins_common.pri)

HEADERS += itemimage.h
SOURCES += itemimage.cpp
FORMS   += itemimagesettings.ui
TARGET   = $$qtLibraryTarget(itemimage)

HEADERS += ../../../src/include/itemeditor.h
SOURCES += ../../../src/itemeditor.cpp

