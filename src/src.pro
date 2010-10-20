TEMPLATE = app
include(../qtsingleapplication/src/qtsingleapplication.pri)

TARGET = ../copyq
DEPENDPATH += .
INCLUDEPATH += . \
    ../qtsingleapplication/src
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
    configurationmanager.h
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
    configurationmanager.cpp
QT += xml
unix:LIBS += -lXi
win32:CONFIG += static
