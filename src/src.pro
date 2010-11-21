TEMPLATE = app

TARGET = ../copyq
DEPENDPATH += .
INCLUDEPATH += .
RESOURCES = copyq.qrc
FORMS += mainwindow.ui \
    actiondialog.ui \
    aboutdialog.ui \
    configurationmanager.ui
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
    app.h
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
    app.cpp
QT += xml network
unix:LIBS += -lXi
win32:CONFIG += static
