equals(WITH_WEBENGINE,1) {
    include(../plugins_common.pri)

    HEADERS += itemweb.h
    SOURCES += itemweb.cpp
    FORMS   += itemwebsettings.ui
    TARGET   = $$qtLibraryTarget(itemweb)

    QT += webenginewidgets
}
!equals(WITH_WEBENGINE,1) {
    TEMPLATE = subdirs
}

