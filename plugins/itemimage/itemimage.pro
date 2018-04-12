include(../plugins_common.pri)

HEADERS += \
    itemimage.h \
    ../../src/item/itemeditor.h
SOURCES += \
    itemimage.cpp \
    ../../src/item/itemeditor.cpp \
    ../../src/common/log.cpp \
    ../../src/common/mimetypes.cpp \
    ../../src/common/temporaryfile.cpp
FORMS   += itemimagesettings.ui
TARGET   = $$qtLibraryTarget(itemimage)

