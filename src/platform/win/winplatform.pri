CONFIG  += static console
LIBS    += -luser32
SOURCES += \
    ../qxt/qxtglobalshortcut_win.cpp \
    platform/win/winplatform.cpp \
    platform/win/winplatformwindow.cpp
USE_QXT = 1
RC_FILE  = copyq.rc

HEADERS += \
    platform/win/winplatformwindow.h
