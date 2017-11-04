equals(WITH_WEBKIT,1) {
    include(../plugins_common.pri)

    HEADERS += itemweb.h
    SOURCES += itemweb.cpp \
        ../../src/common/mimetypes.cpp
        ../../src/common/textdata.cpp
    FORMS   += itemwebsettings.ui
    TARGET   = $$qtLibraryTarget(itemweb)

    lessThan(QT_MAJOR_VERSION, 5) {
        QT += webkit
    }
    !lessThan(QT_MAJOR_VERSION, 5) {
        QT += webkitwidgets
    }
}
!equals(WITH_WEBKIT,1) {
    TEMPLATE = subdirs
}

