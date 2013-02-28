CONFIG  += static console
LIBS    += -luser32
SOURCES += platform/win/winplatform.cpp \
           ../qxt/qxtglobalshortcut_win.cpp
USE_QXT = 1
RC_FILE  = copyq.rc
