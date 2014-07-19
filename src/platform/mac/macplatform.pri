LIBS += -framework Carbon -framework Cocoa
QT += macextras

SOURCES += \
    ../qxt/qxtglobalshortcut_mac.cpp \
    platform/dummy/dummyclipboard.cpp

HEADERS += \
    $$PWD/mactimer.h \
    $$PWD/foregroundbackgroundfilter.h \
    platform/dummy/dummyclipboard.h

OBJECTIVE_SOURCES += \
    $$PWD/macplatform.mm \
    $$PWD/macplatformwindow.mm \
    $$PWD/macactivity.mm \
    $$PWD/copyqpasteboardmime.mm \
    $$PWD/urlpasteboardmime.mm \
    $$PWD/foregroundbackgroundfilter.mm \
    $$PWD/mactimer.mm

USE_QXT = 1
