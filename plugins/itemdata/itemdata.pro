include(../plugins_common.pri)

HEADERS += itemdata.h
SOURCES += itemdata.cpp \
    ../../src/common/log.cpp \
    ../../src/common/mimetypes.cpp \
    ../../src/common/textdata.cpp
FORMS   += itemdatasettings.ui
TARGET   = $$qtLibraryTarget(itemdata)

