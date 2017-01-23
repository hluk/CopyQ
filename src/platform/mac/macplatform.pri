LIBS += -framework Carbon -framework Cocoa
QT += macextras

SOURCES += \
    $$PWD/../../../qxt/qxtglobalshortcut_mac.cpp \
    $$PWD/../dummy/dummyclipboard.cpp

HEADERS += \
    $$PWD/mactimer.h \
    $$PWD/macclipboard.h \
    $$PWD/foregroundbackgroundfilter.h \
    $$PWD/../dummy/dummyclipboard.h

OBJECTIVE_SOURCES += \
    $$PWD/macplatform.mm \
    $$PWD/macplatformwindow.mm \
    $$PWD/macactivity.mm \
    $$PWD/macclipboard.mm \
    $$PWD/copyqpasteboardmime.mm \
    $$PWD/urlpasteboardmime.mm \
    $$PWD/foregroundbackgroundfilter.mm \
    $$PWD/mactimer.mm

USE_QXT = 1
