LIBS += -framework Carbon -framework Cocoa
QT += macextras

SOURCES += \
    ../qxt/qxtglobalshortcut_mac.cpp \
    platform/mac/foregroundbackgroundfilter.cpp

HEADERS += \
    platform/mac/mactimer.h \
    platform/mac/foregroundbackgroundfilter.h


OBJECTIVE_SOURCES += \
    platform/mac/macplatform.mm \
    platform/mac/macactivity.mm \
    platform/mac/copyqpasteboardmime.mm \
    platform/mac/mactimer.mm

USE_QXT = 1
