LIBS += -framework Carbon -framework Cocoa
QT += macextras

SOURCES += \
    ../qxt/qxtglobalshortcut_mac.cpp \

HEADERS += \
    platform/mac/mactimer.h \
    platform/mac/foregroundbackgroundfilter.h

OBJECTIVE_SOURCES += \
    platform/mac/macplatform.mm \
    platform/mac/macplatformwindow.mm \
    platform/mac/macactivity.mm \
    platform/mac/copyqpasteboardmime.mm \
    platform/mac/urlpasteboardmime.mm \
    platform/mac/foregroundbackgroundfilter.mm \
    platform/mac/mactimer.mm

USE_QXT = 1
