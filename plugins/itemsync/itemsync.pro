include(../plugins_common.pri)

HEADERS += \
    itemsync.h \
    filewatcher.h \
    ../../src/gui/iconselectbutton.h \
    ../../src/gui/iconselectdialog.h \
    ../../src/gui/iconwidget.h

SOURCES += \
    itemsync.cpp \
    filewatcher.cpp \
    ../../src/common/config.cpp \
    ../../src/common/log.cpp \
    ../../src/common/mimetypes.cpp \
    ../../src/gui/iconfont.cpp \
    ../../src/gui/iconselectbutton.cpp \
    ../../src/gui/iconselectdialog.cpp \
    ../../src/gui/iconwidget.cpp \
    ../../src/item/serialize.cpp

FORMS += itemsyncsettings.ui

CONFIG(debug, debug|release) {
    SOURCES += tests/itemsynctests.cpp
    HEADERS += tests/itemsynctests.h
}

TARGET = $$qtLibraryTarget(itemsync)

