unix:!macx:!android {
    include(x11/x11platform.pri)
}
win32 {
    include(win/winplatform.pri)
}
macx {
    include(mac/macplatform.pri)
}
!unix|android:!win32 {
    SOURCES += $$PWD/dummy/dummyplatform.cpp
}
unix {
    SOURCES += $$PWD/unix/unixsignalhandler.cpp
    HEADERS += $$PWD/unix/unixsignalhandler.h
}

HEADERS += \
    $$PWD/platformclipboard.h \
    $$PWD/platformwindow.h \
    $$PWD/dummy/dummyclipboard.h

SOURCES += \
    $$PWD/dummy/dummyclipboard.cpp

