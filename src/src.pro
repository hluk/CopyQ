TEMPLATE = app

TARGET = ../copyq
DEPENDPATH += .
INCLUDEPATH += $$PWD
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
    ui/commandwidget.ui \
    ui/configtabappearance.ui
HEADERS += \
    app/app.h \
    app/clipboardclient.h \
    app/clipboardmonitor.h \
    app/clipboardserver.h \
    app/remoteprocess.h \
    common/action.h \
    common/arguments.h \
    common/client_server.h \
    common/command.h \
    common/contenttype.h \
    common/option.h \
    gui/aboutdialog.h \
    gui/actiondialog.h \
    gui/clipboardbrowser.h \
    gui/clipboarddialog.h \
    gui/commandwidget.h \
    gui/configurationmanager.h \
    gui/iconfactory.h \
    gui/mainwindow.h \
    gui/pluginwidget.h \
    gui/shortcutdialog.h \
    gui/tabbar.h \
    gui/tabdialog.h \
    gui/tabwidget.h \
    gui/traymenu.h \
    item/clipboarditem.h \
    item/clipboardmodel.h \
    item/itemdelegate.h \
    item/itemeditor.h \
    item/itemfactory.h \
    item/itemwidget.h \
    platform/dummy/dummyplatform.h \
    platform/platformnativeinterface.h \
    ../qt/bytearrayclass.h \
    ../qt/bytearrayprototype.h \
    scriptable/scriptable.h \
    scriptable/scriptableproxy.h \
    scriptable/scriptableworker.h \
    gui/tabtree.h \
    gui/configtabappearance.h \
    gui/notification.h \
    gui/notificationdaemon.h \
    item/itemeditorwidget.h
SOURCES += \
    app/app.cpp \
    app/clipboardclient.cpp \
    app/clipboardmonitor.cpp \
    app/clipboardserver.cpp \
    app/remoteprocess.cpp \
    common/action.cpp \
    common/arguments.cpp \
    common/client_server.cpp \
    common/option.cpp \
    gui/aboutdialog.cpp \
    gui/actiondialog.cpp \
    gui/clipboardbrowser.cpp \
    gui/clipboarddialog.cpp \
    gui/commandwidget.cpp \
    gui/configurationmanager.cpp \
    gui/iconfactory.cpp \
    gui/mainwindow.cpp \
    gui/pluginwidget.cpp \
    gui/shortcutdialog.cpp \
    gui/tabbar.cpp \
    gui/tabdialog.cpp \
    gui/tabwidget.cpp \
    gui/traymenu.cpp \
    item/clipboarditem.cpp \
    item/clipboardmodel.cpp \
    item/itemdelegate.cpp \
    item/itemeditor.cpp \
    item/itemfactory.cpp \
    item/itemwidget.cpp \
    main.cpp \
    ../qt/bytearrayclass.cpp \
    ../qt/bytearrayprototype.cpp \
    scriptable/scriptable.cpp \
    scriptable/scriptableworker.cpp \
    gui/tabtree.cpp \
    gui/configtabappearance.cpp \
    gui/notification.cpp \
    gui/notificationdaemon.cpp \
    item/itemeditorwidget.cpp

QT += core gui xml network script
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

Debug {
    DEFINES += HAS_TESTS COPYQ_LOG_DEBUG
    QT += testlib
    SOURCES += tests/tests.cpp
    HEADERS += tests/tests.h
}

include(platform/platform.pri)

equals(USE_QXT,1) {
    DEFINES += BUILD_QXT_GUI

    HEADERS += ../qxt/qxtglobal.h
    SOURCES += ../qxt/qxtglobal.cpp

    HEADERS += ../qxt/qxtglobalshortcut.h
    HEADERS += ../qxt/qxtglobalshortcut_p.h
    SOURCES += ../qxt/qxtglobalshortcut.cpp
}

