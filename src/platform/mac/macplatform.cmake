file(GLOB copyq_SOURCES ${copyq_SOURCES}
    platform/mac/*.cpp
    platform/mac/*.mm
    platform/dummy/dummyclipboard.cpp
    ../qxt/qxtglobalshortcut_mac.cpp
    )

include(platform/qxt.cmake)

list(APPEND copyq_qt_modules MacExtras)
