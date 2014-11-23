equals(WITH_WEBKIT,1) {
    include(../plugins_common.pri)

    HEADERS += itemtags.h \
        ../../src/gui/iconwidget.h
    SOURCES += itemtags.cpp \
        ../../src/common/common.cpp \
        ../../src/gui/iconbutton.cpp \
        ../../src/gui/iconfont.cpp
    FORMS   += itemtagssettings.ui
    TARGET   = $$qtLibraryTarget(itemtags)

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

