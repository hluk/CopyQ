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
    ui/pluginwidget.ui \
    ui/tabdialog.ui \
    ui/shortcutdialog.ui \
    ui/commandwidget.ui
HEADERS += \
    include/aboutdialog.h \
    include/actiondialog.h \
    include/action.h \
    include/app.h \
    include/arguments.h \
    include/client_server.h \
    include/clipboardbrowser.h \
    include/clipboardclient.h \
    include/clipboarddialog.h \
    include/clipboarditem.h \
    include/clipboardmodel.h \
    include/clipboardmonitor.h \
    include/clipboardserver.h \
    include/contenttype.h \
    include/command.h \
    include/commandwidget.h \
    include/configurationmanager.h \
    include/iconfactory.h \
    include/itemdelegate.h \
    include/itemeditor.h \
    include/itemfactory.h \
    include/itemwidget.h \
    include/mainwindow.h \
    include/option.h \
    include/pluginwidget.h \
    include/remoteprocess.h \
    include/scriptable.h \
    include/shortcutdialog.h \
    include/tabbar.h \
    include/tabdialog.h \
    include/tabwidget.h \
    ../qt/bytearrayclass.h \
    ../qt/bytearrayprototype.h
SOURCES += \
    aboutdialog.cpp \
    action.cpp \
    actiondialog.cpp \
    app.cpp \
    arguments.cpp \
    client_server.cpp \
    clipboardbrowser.cpp \
    clipboardclient.cpp \
    clipboarddialog.cpp \
    clipboarditem.cpp \
    clipboardmodel.cpp \
    clipboardmonitor.cpp \
    clipboardserver.cpp \
    commandwidget.cpp \
    configurationmanager.cpp \
    iconfactory.cpp \
    itemdelegate.cpp \
    itemeditor.cpp \
    itemfactory.cpp \
    itemwidget.cpp \
    main.cpp \
    mainwindow.cpp \
    option.cpp \
    pluginwidget.cpp \
    remoteprocess.cpp \
    scriptable.cpp \
    shortcutdialog.cpp \
    tabbar.cpp \
    tabdialog.cpp \
    tabwidget.cpp \
    ../qt/bytearrayclass.cpp \
    ../qt/bytearrayprototype.cpp

QT += core gui xml network script
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

Debug {
    DEFINES += HAS_TESTS COPYQ_LOG_DEBUG
    QT += testlib
    SOURCES += tests/tests.cpp
    HEADERS += tests/tests.h
}

lessThan(QT_MAJOR_VERSION, 5): unix | win32 {
    DEFINES += BUILD_QXT_GUI

    HEADERS += ../qxt/qxtglobal.h
    SOURCES += ../qxt/qxtglobal.cpp

    HEADERS += ../qxt/qxtglobalshortcut.h
    HEADERS += ../qxt/qxtglobalshortcut_p.h
    SOURCES += ../qxt/qxtglobalshortcut.cpp

    unix:!macx: SOURCES += ../qxt/qxtglobalshortcut_x11.cpp
    macx:       SOURCES += ../qxt/qxtglobalshortcut_mac.cpp
    win32:      SOURCES += ../qxt/qxtglobalshortcut_win.cpp
}

unix:!macx {
    DEFINES += COPYQ_WS_X11
    LIBS    += -lX11 -lXfixes
}
macx {
    LIBS    += -framework Carbon
}
win32 {
    CONFIG  += static console
    LIBS    += -luser32
    RC_FILE  = copyq.rc
}

