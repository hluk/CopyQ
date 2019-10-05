# Qxt - global shortcuts
set(copyq_SOURCES ${copyq_SOURCES}
    ../qxt/qxtglobalshortcut.cpp
    )
add_definitions( -DQXT_STATIC )
include_directories(SYSTEM ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
