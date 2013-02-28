# X11 requires 'fixes' extension for clipboard to work correctly.
message(STATUS "Building for X11 window system.")
if(NOT X11_Xfixes_FOUND)
    message(FATAL_ERROR "X11 'fixes' extension library is required")
endif(NOT X11_Xfixes_FOUND)

add_definitions( -DCOPYQ_WS_X11 )

file(GLOB copyq_SOURCES ${copyq_SOURCES}
    platform/x11/*.cpp
    ../qxt/qxtglobalshortcut_x11.cpp
    )

if (WITH_QT5)
    include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
endif()

set(USE_QXT TRUE)

set(copyq_LIBRARIES ${copyq_LIBRARIES} ${X11_LIBRARIES} ${X11_Xfixes_LIB})

