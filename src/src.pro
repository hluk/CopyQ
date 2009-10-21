TEMPLATE = app
TARGET = copyq
DEPENDPATH += .
INCLUDEPATH += . ../qtsingleapplication/
RESOURCES = copyq.qrc
FORMS += mainwindow.ui

HEADERS += mainwindow.h \
    clipboardbrowser.h \
    searchbar.h \
    qeditor.h \
    itemdelegate.h

SOURCES += main.cpp \
    mainwindow.cpp \
    clipboardbrowser.cpp \
    searchbar.cpp \
    qeditor.cpp \
    itemdelegate.cpp

QT += webkit xml
LIBS += -lXi

include(../qtsingleapplication/qtsingleapplication.pri)
