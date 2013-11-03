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
    SOURCES += platform/dummy/dummyplatform.cpp
}

