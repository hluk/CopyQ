DEFINES += COPYQ_WS_X11 HAS_X11TEST
LIBS    += -lX11 -lXfixes -lXtst
SOURCES += \
    ../qxt/qxtglobalshortcut_x11.cpp \
    $$PWD/x11platform.cpp \
    $$PWD/x11platformwindow.cpp \
    $$PWD/x11platformclipboard.cpp \
    platform/dummy/dummyclipboard.cpp
USE_QXT = 1

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += gui-private
}

HEADERS += \
    $$PWD/x11platformwindow.h \
    $$PWD/x11platformclipboard.h \
    platform/dummy/dummyclipboard.h

