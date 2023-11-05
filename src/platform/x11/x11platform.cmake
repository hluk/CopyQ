file(GLOB copyq_SOURCES ${copyq_SOURCES}
    platform/x11/x11info.cpp
    platform/x11/x11platform.cpp
    platform/x11/x11platformclipboard.cpp
    platform/dummy/dummyclipboard.cpp
    platform/platformcommon.cpp
    )

add_definitions( -DHAS_MOUSE_SELECTIONS )
add_definitions( -DCOPYQ_MOVE_TO_WORKSPACE )

OPTION(WITH_X11 "Enable X11 support (global shortcuts, getting window titles)" ON)
if (WITH_X11)
    add_definitions( -DCOPYQ_WITH_X11 )

    # X11 requires 'fixes' extension for clipboard to work correctly.
    message(STATUS "Building for X11 window system.")
    if(NOT X11_Xfixes_FOUND)
        message(FATAL_ERROR "X11 'fixes' extension library is required")
    endif(NOT X11_Xfixes_FOUND)

    if(X11_XTest_FOUND)
        add_definitions( -DHAS_X11TEST )
        list(APPEND copyq_DEFINITIONS HAS_X11TEST)
        list(APPEND copyq_LIBRARIES ${X11_XTest_LIB})
    else(X11_XTest_FOUND)
        message(WARNING "X11 'TEST' extension library is needed to be able to"
                        " automatically paste to some windows!")
    endif(X11_XTest_FOUND)

    file(GLOB copyq_SOURCES ${copyq_SOURCES}
        platform/x11/x11platformwindow.cpp
        ../qxt/qxtglobalshortcut_x11.cpp
        )

    set(USE_QXT TRUE)

    set(copyq_LIBRARIES ${copyq_LIBRARIES} ${X11_LIBRARIES} ${X11_Xfixes_LIB})

    if(WITH_QT6)
        list(APPEND copyq_LIBRARIES Qt::GuiPrivate)
    else()
        list(APPEND copyq_qt_modules X11Extras)
    endif()
endif()

# Wayland clipboard
find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/platform/x11/systemclipboard)
add_subdirectory(platform/x11/systemclipboard)
set_target_properties(systemclipboard PROPERTIES COMPILE_FLAGS "-Wno-old-style-cast")
list(APPEND copyq_LIBRARIES systemclipboard)
