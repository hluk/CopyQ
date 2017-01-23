DEFINES += COPYQ_WS_X11 HAS_X11TEST
LIBS    += -lX11 -lXfixes -lXtst
SOURCES += \
    $$PWD/../../../qxt/qxtglobalshortcut_x11.cpp \
    $$PWD/clipboardspy.cpp \
    $$PWD/x11platform.cpp \
    $$PWD/x11platformwindow.cpp \
    $$PWD/x11platformclipboard.cpp \
    $$PWD/../dummy/dummyclipboard.cpp \
    $$PWD/../dummy/dummyplatformsystemmutex.cpp \
    $$PWD/../common/pastewithctrlv.cpp
USE_QXT = 1

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += gui-private
}

HEADERS += \
    $$PWD/clipboardspy.h \
    $$PWD/x11platformwindow.h \
    $$PWD/x11platformclipboard.h \
    $$PWD/../dummy/dummyclipboard.h

