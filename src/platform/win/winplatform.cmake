# File icon on Windows
set(copyq_RC copyq.rc)

file(GLOB copyq_SOURCES ${copyq_SOURCES}
    platform/win/winplatform.cpp
    platform/win/winplatformclipboard.cpp
    platform/win/winplatformwindow.cpp
    platform/dummy/dummyclipboard.cpp
    platform/platformcommon.cpp
    ../qxt/qxtglobalshortcut_win.cpp
    )

include(platform/qxt.cmake)

# Omit opening extra console window on Windows.
set(copyq_COMPILE_EXE
    WIN32
    ${copyq_COMPILE_EXE}
    ${copyq_RC}
    )

set(copyq_PROPERTIES_LIB WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

if (MSVC)
    set(copyq_LINK_FLAGS_EXE ${copyq_LINK_FLAGS_EXE} "/ENTRY:mainCRTStartup")
endif()
