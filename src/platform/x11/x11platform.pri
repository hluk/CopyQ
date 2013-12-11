DEFINES += COPYQ_WS_X11 HAS_X11TEST
LIBS    += -lX11 -lXfixes -lXtst
SOURCES += \
    ../qxt/qxtglobalshortcut_x11.cpp \
    platform/x11/x11platform.cpp \
    platform/x11/x11platformwindow.cpp
USE_QXT = 1

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += gui-private
}

HEADERS += \
    platform/x11/x11platformwindow.h \
    platform/x11/x11displayguard.h

