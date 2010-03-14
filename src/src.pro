TEMPLATE = app
TARGET = ../copyq
DEPENDPATH += .
INCLUDEPATH += . \
    ../qtsingleapplication/
RESOURCES = copyq.qrc
FORMS += mainwindow.ui \
    actiondialog.ui \
    aboutdialog.ui
HEADERS += mainwindow.h \
    clipboardbrowser.h \
    qeditor.h \
    actiondialog.h \
    clipboardmodel.h \
    itemdelegate.h \
    action.h
SOURCES += main.cpp \
    mainwindow.cpp \
    clipboardbrowser.cpp \
    qeditor.cpp \
    actiondialog.cpp \
    clipboardmodel.cpp \
    itemdelegate.cpp \
    action.cpp
QT += xml
unix:LIBS += -lXi
win32:CONFIG += static
include(qtsingleapplication/qtsingleapplication.pri)
