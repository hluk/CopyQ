file(GLOB copyq_SOURCES ${copyq_SOURCES}
    platform/mac/*.cpp
    platform/mac/*.mm
    platform/dummy/dummyclipboard.cpp
    ../qxt/qxtglobalshortcut_mac.cpp
    )

set(USE_QXT TRUE)

set(copyq_Qt5_Modules ${copyq_Qt5_Modules} MacExtras)

