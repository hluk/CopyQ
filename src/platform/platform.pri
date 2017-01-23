unix:!macx:!android {
    include(x11/x11platform.pri)
}
win32 {
    include(win/winplatform.pri)
}
macx {
    include(mac/macplatform.pri)
}
android {
    include(android/androidplatform.pri)
}
!unix:!win32:!android {
    SOURCES += \
        $$PWD/dummy/dummyplatform.cpp \
        $$PWD/dummy/dummyplatformsystemmutex.cpp \
        $$PWD/dummy/dummyclipboard.cpp
    HEADERS += \
        $$PWD/dummy/dummyclipboard.h
}
unix {
    SOURCES += $$PWD/unix/unixsignalhandler.cpp
    HEADERS += $$PWD/unix/unixsignalhandler.h
}
