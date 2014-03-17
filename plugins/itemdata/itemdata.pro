include(../plugins_common.pri)

HEADERS += itemdata.h
SOURCES += itemdata.cpp \
    ../../src/common/common.cpp \
    ../../src/common/log.cpp \
    ../../src/common/mimetypes.cpp
FORMS   += itemdatasettings.ui
TARGET   = $$qtLibraryTarget(itemdata)

