CONFIG  += static console
LIBS    += -luser32
SOURCES += \
    ../qxt/qxtglobalshortcut_win.cpp \
    $$PWD/winplatform.cpp \
    $$PWD/winplatformwindow.cpp \
    $$PWD/winplatformclipboard.cpp \
    platform/dummy/dummyclipboard.cpp \
    platform/platformcommon.cpp
USE_QXT = 1
RC_FILE  = copyq.rc

HEADERS += \
    $$PWD/winplatformwindow.h \
    $$PWD/winplatformclipboard.h \
    platform/dummy/dummyclipboard.h
