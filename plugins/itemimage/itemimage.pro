include(../plugins_common.pri)

HEADERS += \
    itemimage.h \
    ../../src/item/itemeditor.h
SOURCES += \
    ../../src/common/log.cpp \
    ../../src/common/mimetypes.cpp \
    ../../src/item/itemeditor.cpp
FORMS   += itemimagesettings.ui
TARGET   = $$qtLibraryTarget(itemimage)

