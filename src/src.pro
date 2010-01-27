TEMPLATE = app
TARGET = ../copyq
DEPENDPATH += .
INCLUDEPATH += . \
    ../qtsingleapplication/
RESOURCES = copyq.qrc
FORMS += mainwindow.ui \
    actiondialog.ui
HEADERS += mainwindow.h \
    clipboardbrowser.h \
    qeditor.h \
    actiondialog.h \
    clipboardmodel.h \
    itemdelegate.h
SOURCES += main.cpp \
    mainwindow.cpp \
    clipboardbrowser.cpp \
    qeditor.cpp \
    actiondialog.cpp \
    clipboardmodel.cpp \
    itemdelegate.cpp
QT += xml
LIBS += -lXi
include(qtsingleapplication/qtsingleapplication.pri)
