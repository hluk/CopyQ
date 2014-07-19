CONFIG  += static console
LIBS    += -luser32
SOURCES += \
    ../qxt/qxtglobalshortcut_win.cpp \
    $$PWD/winplatform.cpp \
    $$PWD/winplatformwindow.cpp \
    platform/dummy/dummyclipboard.cpp
USE_QXT = 1
RC_FILE  = copyq.rc

HEADERS += \
    $$PWD/winplatformwindow.h \
    platform/dummy/dummyclipboard.h
