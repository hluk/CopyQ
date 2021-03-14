file(GLOB copyq_SOURCES ${copyq_SOURCES}
    platform/mac/*.cpp
    platform/mac/*.mm
    platform/dummy/dummyclipboard.cpp
    platform/platformcommon.cpp
    ../qxt/qxtglobalshortcut_mac.cpp
    )

set(USE_QXT TRUE)

list(APPEND copyq_qt_modules MacExtras)
