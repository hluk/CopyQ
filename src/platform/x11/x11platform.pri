DEFINES += COPYQ_WS_X11 HAS_X11TEST
LIBS    += -lX11 -lXfixes -lXtst
SOURCES += platform/x11/x11platform.cpp \
           ../qxt/qxtglobalshortcut_x11.cpp
USE_QXT = 1

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += gui-private
}

