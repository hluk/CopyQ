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

set(USE_QXT TRUE)

# Omit opening extra console window on Windows.
set(copyq_windows_no_console WIN32)
list(APPEND copyq_COMPILE
    ${copyq_COMPILE}
    ${copyq_RC}
    )

if (MSVC)
    set(copyq_LINK_FLAGS ${copyq_LINK_FLAGS} "/ENTRY:mainCRTStartup")
endif()


# Console-subsystem wrapper (copyq.com).
# Windows prefers .com over .exe, so "copyq" in a terminal runs this
# wrapper which launches the GUI-subsystem copyq.exe with inherited
# console handles.  This gives proper shell integration: the shell
# waits, stdout works, piping works, exit codes propagate.
add_executable(copyq-console-wrapper
    platform/win/copyq_console_wrapper.cpp
    ${copyq_RC}
)
set_target_properties(copyq-console-wrapper PROPERTIES
    OUTPUT_NAME copyq
    SUFFIX .com
    WIN32_EXECUTABLE FALSE
)
install(TARGETS copyq-console-wrapper
    RUNTIME DESTINATION . COMPONENT Runtime
)
