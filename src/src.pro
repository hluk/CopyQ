TEMPLATE = app

TARGET = ../copyq
DEPENDPATH += .
INCLUDEPATH += $$PWD
RESOURCES = copyq.qrc
FORMS += \
    ui/aboutdialog.ui \
    ui/actiondialog.ui \
    ui/clipboarddialog.ui \
    ui/commandwidget.ui \
    ui/configtabappearance.ui \
    ui/configurationmanager.ui \
    ui/itemorderlist.ui \
    ui/mainwindow.ui \
    ui/pluginwidget.ui \
    ui/shortcutdialog.ui \
    ui/tabdialog.ui \
    ui/configtabshortcuts.ui \
    ui/shortcutswidget.ui
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
    common/common.h \
    gui/aboutdialog.h \
    gui/actiondialog.h \
    gui/clipboardbrowser.h \
    gui/clipboarddialog.h \
    gui/commandwidget.h \
    gui/configtabappearance.h \
    gui/configtabshortcuts.h \
    gui/configurationmanager.h \
    gui/fancylineedit.h \
    gui/filterlineedit.h \
    gui/iconfactory.h \
    gui/iconselectbutton.h \
    gui/iconwidget.h \
    gui/itemorderlist.h \
    gui/mainwindow.h \
    gui/notificationdaemon.h \
    gui/notification.h \
    gui/pluginwidget.h \
    gui/shortcutbutton.h \
    gui/shortcutdialog.h \
    gui/shortcutswidget.h \
    gui/tabbar.h \
    gui/tabdialog.h \
    gui/tabtree.h \
    gui/tabwidget.h \
    gui/traymenu.h \
    item/clipboarditem.h \
    item/clipboardmodel.h \
    item/encrypt.h \
    item/itemdelegate.h \
    item/itemeditor.h \
    item/itemeditorwidget.h \
    item/itemfactory.h \
    item/itemwidget.h \
    item/serialize.h \
    platform/dummy/dummyplatform.h \
    platform/platformnativeinterface.h \
    ../qt/bytearrayclass.h \
    ../qt/bytearrayprototype.h \
    scriptable/scriptable.h \
    scriptable/scriptableproxy.h \
    scriptable/scriptableworker.h \
    gui/iconfont.h
SOURCES += \
    app/app.cpp \
    app/clipboardclient.cpp \
    app/clipboardmonitor.cpp \
    app/clipboardserver.cpp \
    app/remoteprocess.cpp \
    common/action.cpp \
    common/arguments.cpp \
    common/client_server.cpp \
    common/common.cpp \
    common/option.cpp \
    gui/aboutdialog.cpp \
    gui/actiondialog.cpp \
    gui/clipboardbrowser.cpp \
    gui/clipboarddialog.cpp \
    gui/commandwidget.cpp \
    gui/configtabappearance.cpp \
    gui/configtabshortcuts.cpp \
    gui/configurationmanager.cpp \
    gui/fancylineedit.cpp \
    gui/filterlineedit.cpp \
    gui/iconfactory.cpp \
    gui/iconselectbutton.cpp \
    gui/iconwidget.cpp \
    gui/itemorderlist.cpp \
    gui/mainwindow.cpp \
    gui/notification.cpp \
    gui/notificationdaemon.cpp \
    gui/pluginwidget.cpp \
    gui/shortcutbutton.cpp \
    gui/shortcutdialog.cpp \
    gui/shortcutswidget.cpp \
    gui/tabbar.cpp \
    gui/tabdialog.cpp \
    gui/tabtree.cpp \
    gui/tabwidget.cpp \
    gui/traymenu.cpp \
    item/clipboarditem.cpp \
    item/clipboardmodel.cpp \
    item/encrypt.cpp \
    item/itemdelegate.cpp \
    item/itemeditor.cpp \
    item/itemeditorwidget.cpp \
    item/itemfactory.cpp \
    item/itemwidget.cpp \
    item/serialize.cpp \
    main.cpp \
    ../qt/bytearrayclass.cpp \
    ../qt/bytearrayprototype.cpp \
    scriptable/scriptable.cpp \
    scriptable/scriptableworker.cpp \
    gui/iconfont.cpp

macx {
    # Only Intel binaries are accepted so force this
    CONFIG += x86

    # Minimum OS X version for submission is 10.6.6
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.6

    # Copy the custom Info.plist to the app bundle
    QMAKE_INFO_PLIST = ../shared/Info.plist

    # Icon is mandatory for submission
    ICON = images/icon.icns

    # Add plugins to the app bundle
    COPYQ_PLUGINS.files = $$files(../plugins/*.dylib)
    COPYQ_PLUGINS.path = Contents/Resources/plugins/
    QMAKE_BUNDLE_DATA += COPYQ_PLUGINS
}

QT += core gui xml network script svg
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG(debug, debug|release) {
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

