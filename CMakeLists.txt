cmake_minimum_required(VERSION 2.8)
project(copyq)

OPTION(WITH_QT5 "Qt5 support" OFF)
OPTION(WITH_TESTS "Run test cases from command line" OFF)
OPTION(WITH_PLUGINS "Compile plugins" ON)

# If Qt4 unavailable use Qt5.
if (NOT WITH_QT5)
    find_package(Qt4)
    if (NOT QT4_FOUND)
        # Try different executable name.
        set(QT_QMAKE_EXECUTABLE "qmake-qt4")
        find_package(Qt4)
        if (NOT QT4_FOUND)
            message(FATAL_ERROR
                    "Qt 4 is unavailable. To compile with Qt 5 use -DWITH_QT5=TRUE.\n"
                    "Note: Qt version 5.0.2 and earlier can be unstable on some systems.")
        endif()
    endif()
endif()

if (WITH_QT5)
    cmake_minimum_required(VERSION 2.8.8)
    find_package(Qt5Widgets REQUIRED)
    message(STATUS "Building with Qt 5.")
else()
    message(STATUS "Building with Qt 4.")
endif()

set(copyq_ICON        src/images/logo.svg)
set(copyq_ICON_NORMAL src/images/icon.svg)
set(copyq_ICON_BUSY   src/images/icon-running.svg)
set(copyq_ICON_DISABLED src/images/icon-disabled.svg)
set(copyq_ICON_DISABLED_BUSY   src/images/icon-disabled-running.svg)
set(copyq_DESKTOP     shared/copyq.desktop)

# Be more strict while compiling debugging version
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-long-long")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra -Wall -pedantic")
endif()

# Tests always on with debug version
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(WITH_TESTS ON)
    add_definitions( -DCOPYQ_LOG_DEBUG )
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(copyq_ICON_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
    add_definitions( -DCOPYQ_ICON_PREFIX="${copyq_ICON_INSTALL_PREFIX}/copyq" )
    install(FILES ${copyq_ICON}        DESTINATION ${copyq_ICON_INSTALL_PREFIX} RENAME copyq.svg)
    install(FILES ${copyq_ICON_NORMAL} DESTINATION ${copyq_ICON_INSTALL_PREFIX} RENAME copyq-normal.svg)
    install(FILES ${copyq_ICON_BUSY}   DESTINATION ${copyq_ICON_INSTALL_PREFIX} RENAME copyq-busy.svg)
    install(FILES ${copyq_ICON_DISABLED} DESTINATION ${copyq_ICON_INSTALL_PREFIX} RENAME copyq-disabled.svg)
    install(FILES ${copyq_ICON_DISABLED_BUSY}   DESTINATION ${copyq_ICON_INSTALL_PREFIX} RENAME copyq-disabled-busy.svg)
    install(FILES ${copyq_DESKTOP}     DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    set(copyq_plugin_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/copyq/plugins")

    set(copyq_THEME_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/copyq/themes")
    add_definitions( -DCOPYQ_THEME_PREFIX="${copyq_THEME_INSTALL_PREFIX}" )
    file(GLOB copyq_THEMES shared/themes/*.ini)
    install(FILES ${copyq_THEMES} DESTINATION ${copyq_THEME_INSTALL_PREFIX})
else()
    set(copyq_plugin_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/plugins")
endif()

add_subdirectory(src)

if (WITH_PLUGINS)
    add_subdirectory(plugins)
endif()

