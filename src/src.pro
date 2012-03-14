TEMPLATE = app

TARGET = ../copyq
DEPENDPATH += .
INCLUDEPATH += .
RESOURCES = copyq.qrc
FORMS += mainwindow.ui \
    actiondialog.ui \
    aboutdialog.ui \
    configurationmanager.ui \
    clipboarddialog.ui \
    tabdialog.ui \
    shortcutdialog.ui \
    commandwidget.ui
HEADERS += mainwindow.h \
    clipboardbrowser.h \
    qeditor.h \
    actiondialog.h \
    clipboardmodel.h \
    itemdelegate.h \
    action.h \
    client_server.h \
    aboutdialog.h \
    clipboarditem.h \
    clipboardmonitor.h \
    configurationmanager.h \
    clipboardserver.h \
    clipboardclient.h \
    app.h \
    arguments.h \
    clipboarddialog.h \
    tabdialog.h \
    shortcutdialog.h \
    commandwidget.h
SOURCES += main.cpp \
    mainwindow.cpp \
    clipboardbrowser.cpp \
    qeditor.cpp \
    actiondialog.cpp \
    clipboardmodel.cpp \
    itemdelegate.cpp \
    client_server.cpp \
    action.cpp \
    aboutdialog.cpp \
    clipboarditem.cpp \
    clipboardmonitor.cpp \
    configurationmanager.cpp \
    clipboardserver.cpp \
    clipboardclient.cpp \
    app.cpp \
    arguments.cpp \
    clipboarddialog.cpp \
    tabdialog.cpp \
    shortcutdialog.cpp \
    commandwidget.cpp

QT += core gui xml network

DEFINES += BUILD_QXT_GUI

HEADERS  += ../qxt/qxtglobal.h
SOURCES  += ../qxt/qxtglobal.cpp

HEADERS  += ../qxt/qxtglobalshortcut.h
HEADERS  += ../qxt/qxtglobalshortcut_p.h
SOURCES  += ../qxt/qxtglobalshortcut.cpp

unix:!macx {
    SOURCES += ../qxt/qxtglobalshortcut_x11.cpp
    LIBS    += -lX11 -lXfixes
}
macx {
    SOURCES += ../qxt/qxtglobalshortcut_mac.cpp
    LIBS    += -framework Carbon
}
win32 {
    SOURCES += ../qxt/qxtglobalshortcut_win.cpp
    CONFIG  += static console
    LIBS    += -luser32
    RC_FILE  = copyq.rc
}
