include("../common.pri")

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
    ui/importexportdialog.ui \
    ui/itemorderlist.ui \
    ui/mainwindow.ui \
    ui/pluginwidget.ui \
    ui/shortcutdialog.ui \
    ui/tabdialog.ui \
    ui/configtabshortcuts.ui \
    ui/shortcutswidget.ui \
    ui/processmanagerdialog.ui \
    ui/commanddialog.ui \
    ui/commandedit.ui \
    ui/addcommanddialog.ui \
    ui/logdialog.ui
HEADERS += \
    app/app.h \
    app/applicationexceptionhandler.h \
    app/clipboardclient.h \
    app/clipboardmonitor.h \
    app/clipboardserver.h \
    common/action.h \
    common/actionoutput.h \
    common/client_server.h \
    common/clientsocket.h \
    common/command.h \
    common/common.h \
    common/contenttype.h \
    common/globalshortcutcommands.h \
    common/option.h \
    common/predefinedcommands.h \
    common/server.h \
    gui/aboutdialog.h \
    gui/actiondialog.h \
    gui/actionhandler.h \
    gui/clipboardbrowser.h \
    gui/clipboardbrowserplaceholder.h \
    gui/clipboarddialog.h \
    gui/clipboardspy.h \
    gui/commandwidget.h \
    gui/configtabappearance.h \
    gui/configtabshortcuts.h \
    gui/configurationmanager.h \
    gui/importexportdialog.h \
    gui/execmenu.h \
    gui/fancylineedit.h \
    gui/filterlineedit.h \
    gui/iconfactory.h \
    gui/iconfont.h \
    gui/iconselectbutton.h \
    gui/icons.h \
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
    item/itemdelegate.h \
    item/itemeditor.h \
    item/itemeditorwidget.h \
    item/itemfactory.h \
    item/itemwidget.h \
    item/persistentdisplayitem.h \
    item/serialize.h \
    platform/dummy/dummyplatform.h \
    platform/platformnativeinterface.h \
    ../qt/bytearrayclass.h \
    ../qt/bytearrayprototype.h \
    scriptable/commandhelp.h \
    scriptable/fileclass.h \
    scriptable/fileprototype.h \
    scriptable/scriptableclass.h \
    scriptable/scriptable.h \
    scriptable/scriptableproxy.h \
    scriptable/temporaryfileclass.h \
    scriptable/temporaryfileprototype.h \
    tests/testinterface.h \
    app/client.h \
    common/mimetypes.h \
    common/log.h \
    common/commandstatus.h \
    common/monitormessagecode.h \
    common/settings.h \
    common/temporarysettings.h \
    common/config.h \
    gui/processmanagerdialog.h \
    gui/iconselectdialog.h \
    gui/commanddialog.h \
    gui/commandedit.h \
    gui/commandsyntaxhighlighter.h \
    gui/commandcompleter.h \
    gui/commandhelpbutton.h \
    scriptable/dirclass.h \
    scriptable/dirprototype.h \
    gui/commandaction.h \
    gui/addcommanddialog.h \
    gui/filtercompleter.h \
    common/sleeptimer.h \
    tests/test_utils.h \
    gui/filedialog.h \
    gui/windowgeometryguard.h \
    gui/logdialog.h \
    common/appconfig.h \
    gui/tabicons.h \
    item/itemstore.h \
    gui/theme.h \
    gui/menuitems.h
SOURCES += \
    app/app.cpp \
    app/applicationexceptionhandler.cpp \
    app/clipboardclient.cpp \
    app/clipboardmonitor.cpp \
    app/clipboardserver.cpp \
    common/action.cpp \
    common/actionoutput.cpp \
    common/client_server.cpp \
    common/clientsocket.cpp \
    common/common.cpp \
    common/commandstore.cpp \
    common/display.cpp \
    common/globalshortcutcommands.cpp \
    common/messagehandlerforqt.cpp \
    common/option.cpp \
    common/predefinedcommands.cpp \
    common/server.cpp \
    common/shortcuts.cpp \
    common/textdata.cpp \
    gui/aboutdialog.cpp \
    gui/actiondialog.cpp \
    gui/actionhandler.cpp \
    gui/clipboardbrowser.cpp \
    gui/clipboardbrowserplaceholder.cpp \
    gui/clipboarddialog.cpp \
    gui/clipboardspy.cpp \
    gui/commandwidget.cpp \
    gui/configtabappearance.cpp \
    gui/configtabshortcuts.cpp \
    gui/configurationmanager.cpp \
    gui/importexportdialog.cpp \
    gui/execmenu.cpp \
    gui/fancylineedit.cpp \
    gui/filterlineedit.cpp \
    gui/iconfactory.cpp \
    gui/iconfont.cpp \
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
    item/itemdelegate.cpp \
    item/itemeditor.cpp \
    item/itemeditorwidget.cpp \
    item/itemfactory.cpp \
    item/itemwidget.cpp \
    item/persistentdisplayitem.cpp \
    item/serialize.cpp \
    main.cpp \
    ../qt/bytearrayclass.cpp \
    ../qt/bytearrayprototype.cpp \
    scriptable/commandhelp.cpp \
    scriptable/fileclass.cpp \
    scriptable/fileprototype.cpp \
    scriptable/scriptableclass.cpp \
    scriptable/scriptable.cpp \
    scriptable/scriptableproxy.cpp \
    scriptable/temporaryfileclass.cpp \
    scriptable/temporaryfileprototype.cpp \
    app/client.cpp \
    common/mimetypes.cpp \
    common/log.cpp \
    common/settings.cpp \
    common/temporarysettings.cpp \
    common/config.cpp \
    gui/processmanagerdialog.cpp \
    gui/iconselectdialog.cpp \
    gui/commanddialog.cpp \
    gui/commandedit.cpp \
    gui/commandsyntaxhighlighter.cpp \
    gui/commandcompleter.cpp \
    gui/commandhelpbutton.cpp \
    scriptable/dirclass.cpp \
    scriptable/dirprototype.cpp \
    gui/commandaction.cpp \
    gui/addcommanddialog.cpp \
    gui/filtercompleter.cpp \
    gui/filedialog.cpp \
    gui/windowgeometryguard.cpp \
    gui/logdialog.cpp \
    common/appconfig.cpp \
    gui/tabicons.cpp \
    item/itemstore.cpp \
    gui/theme.cpp \
    gui/menuitems.cpp

macx {
    # Copy the custom Info.plist to the app bundle
    QMAKE_INFO_PLIST = ../shared/Info.plist

    # Icon is mandatory for submission
    ICON = images/icon.icns
}

QT += core gui xml network script svg
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG(debug, debug|release) {
    CONFIG += tests
    DEFINES += COPYQ_DEBUG
}

CONFIG(tests) {
    DEFINES += HAS_TESTS
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

