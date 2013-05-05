equals(WITH_WEBKIT,0) {
    TEMPLATE = subdirs
}
!equals(WITH_WEBKIT,0) {
    include(../plugins_common.pri)

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

