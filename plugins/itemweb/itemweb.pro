TEMPLATE = lib

WITH_WEBKIT {
    include(../plugins_common.pri)

    DEFINES += HAS_WEBKIT

    HEADERS += itemweb.h
    SOURCES += itemweb.cpp
    TARGET   = $$qtLibraryTarget(itemweb)

    lessThan(QT_MAJOR_VERSION, 5) {
        QT += webkit
    }
    !lessThan(QT_MAJOR_VERSION, 5) {
        QT += webkitwidgets
    }
}

