include(../plugins_common.pri)

HEADERS += itemimage.h
SOURCES += itemimage.cpp \
    ../../src/common/mimetypes.cpp
FORMS   += itemimagesettings.ui
TARGET   = $$qtLibraryTarget(itemimage)

HEADERS += ../../src/item/itemeditor.h
SOURCES += ../../src/item/itemeditor.cpp

