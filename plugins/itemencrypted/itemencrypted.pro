include(../plugins_common.pri)

HEADERS += itemencrypted.h \
    ../../src/gui/iconwidget.h
SOURCES += itemencrypted.cpp
SOURCES += \
    ../../src/common/config.cpp \
    ../../src/common/log.cpp \
    ../../src/common/mimetypes.cpp \
    ../../src/common/shortcuts.cpp \
    ../../src/common/textdata.cpp \
    ../../src/gui/iconfont.cpp \
    ../../src/gui/iconwidget.cpp \
    ../../src/item/serialize.cpp
FORMS   += itemencryptedsettings.ui

CONFIG(debug, debug|release) {
    SOURCES += tests/itemencryptedtests.cpp
    HEADERS += tests/itemencryptedtests.h
}

TARGET   = $$qtLibraryTarget(itemencrypted)

