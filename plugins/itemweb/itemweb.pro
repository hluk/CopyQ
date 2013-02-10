TEMPLATE = lib

WITH_WEBKIT {
    DEFINES     += HAS_WEBKIT

    CONFIG      += plugin
    INCLUDEPATH += ../../src/include
    HEADERS      = itemweb.h \
                   ../../src/include/itemwidget.h
    SOURCES      = itemweb.cpp \
                   ../../src/itemwidget.cpp
    TARGET       = $$qtLibraryTarget(itemweb)
    DESTDIR      = ../

    lessThan(QT_MAJOR_VERSION, 5) {
        QT += webkit
    }
    !lessThan(QT_MAJOR_VERSION, 5) {
        QT += webkitwidgets
    }
}

