include(../plugins_common.pri)

HEADERS += itemtags.h \
    ../../src/gui/iconselectbutton.h \
    ../../src/gui/iconselectdialog.h
SOURCES += itemtags.cpp \
    ../../src/common/config.cpp \
    ../../src/common/mimetypes.cpp \
    ../../src/common/textdata.cpp \
    ../../src/gui/iconselectbutton.cpp \
    ../../src/gui/iconselectdialog.cpp \
    ../../src/gui/iconfont.cpp
FORMS   += itemtagssettings.ui

CONFIG(debug, debug|release) {
    SOURCES += tests/itemtagstests.cpp
    HEADERS += tests/itemtagstests.h
}

TARGET   = $$qtLibraryTarget(itemtags)

