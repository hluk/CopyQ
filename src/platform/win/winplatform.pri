CONFIG  += static console
LIBS    += -luser32
SOURCES += \
    $$PWD/../../../qxt/qxtglobalshortcut_win.cpp \
    $$PWD/winplatform.cpp \
    $$PWD/winplatformwindow.cpp \
    $$PWD/winplatformclipboard.cpp \
    $$PWD/../dummy/dummyclipboard.cpp \
    $$PWD/../dummy/dummyplatformsystemmutex.cpp \
    $$PWD/../common/pastewithctrlv.cpp
USE_QXT = 1
RC_FILE  = copyq.rc

HEADERS += \
    $$PWD/winplatformwindow.h \
    $$PWD/winplatformclipboard.h \
    $$PWD/../dummy/dummyclipboard.h
