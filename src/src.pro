TEMPLATE = app

TARGET = ../copyq
DEPENDPATH += .
INCLUDEPATH += include
RESOURCES = copyq.qrc
FORMS += \
    ui/mainwindow.ui \
    ui/actiondialog.ui \
    ui/aboutdialog.ui \
    ui/configurationmanager.ui \
    ui/clipboarddialog.ui \
    ui/tabdialog.ui \
    ui/shortcutdialog.ui \
    ui/commandwidget.ui
HEADERS += \
    include/mainwindow.h \
    include/clipboardbrowser.h \
    include/qeditor.h \
    include/actiondialog.h \
    include/clipboardmodel.h \
    include/itemdelegate.h \
    include/action.h \
    include/client_server.h \
    include/aboutdialog.h \
    include/clipboarditem.h \
    include/clipboardmonitor.h \
    include/configurationmanager.h \
    include/clipboardserver.h \
    include/clipboardclient.h \
    include/app.h \
    include/arguments.h \
    include/clipboarddialog.h \
    include/tabbar.h \
    include/tabdialog.h \
    include/tabwidget.h \
    include/shortcutdialog.h \
    include/commandwidget.h \
    include/option.h
SOURCES += \
    main.cpp \
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
    tabbar.cpp \
    tabdialog.cpp \
    tabwidget.cpp \
    shortcutdialog.cpp \
    commandwidget.cpp \
    option.cpp

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

OTHER_FILES += \
    styles/default.css
