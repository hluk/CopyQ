include(../plugins_common.pri)

HEADERS += itemtext.h
SOURCES += itemtext.cpp \
    ../../src/common/mimetypes.cpp \
    ../../src/common/textdata.cpp
FORMS   += itemtextsettings.ui
TARGET   = $$qtLibraryTarget(itemtext)

