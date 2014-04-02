cmake_minimum_required(VERSION 2.8.6)
project(copyq)

# Options (cmake -LH)
OPTION(WITH_QT5 "Qt5 support" OFF)
OPTION(WITH_TESTS "Run test cases from command line" OFF)
OPTION(WITH_PLUGINS "Compile plugins" ON)
# Linux-specific options
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(PLUGIN_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${CMAKE_SHARED_MODULE_PREFIX}/copyq/plugins" CACHE PATH "Install path for plugins")
    set(ICON_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps" CACHE PATH "Install path for icons")
    set(THEME_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/copyq/themes" CACHE PATH "Install path for themes")
    set(DESKTOP_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/applications" CACHE PATH "Install path for \"copyq.desktop\"")
    set(TRANSLATION_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/copyq/translations" CACHE PATH "Install path for translations")
endif()

set(CMAKE_AUTOMOC ON)

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
set(copyq_DESKTOP     shared/copyq.desktop)

# Be more strict while compiling debugging version
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-long-long")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra -Wall -pedantic")
endif()

# Tests always on with debug version
if(CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS "Building with tests.")

    set(WITH_TESTS ON)
    add_definitions( -DHAS_TESTS -DCOPYQ_LOG_DEBUG )

    if (WITH_QT5)
        list(APPEND copyq_Qt5_Modules Test)
    else()
        set(QT_USE_QTTEST TRUE)
    endif()
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(FILES ${copyq_ICON}        DESTINATION ${ICON_INSTALL_PREFIX} RENAME copyq.svg)
    install(FILES ${copyq_ICON_NORMAL} DESTINATION ${ICON_INSTALL_PREFIX} RENAME copyq-normal.svg)
    install(FILES ${copyq_ICON_BUSY}   DESTINATION ${ICON_INSTALL_PREFIX} RENAME copyq-busy.svg)
    install(FILES ${copyq_DESKTOP}     DESTINATION ${DESKTOP_INSTALL_PREFIX})

    set(copyq_THEME_INSTALL_PREFIX ${THEME_INSTALL_PREFIX})
    file(GLOB copyq_THEMES shared/themes/*.ini)
    install(FILES ${copyq_THEMES} DESTINATION ${THEME_INSTALL_PREFIX})

    add_definitions( -DCOPYQ_ICON_PREFIX="${ICON_INSTALL_PREFIX}/copyq" )
    add_definitions( -DCOPYQ_THEME_PREFIX="${THEME_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_PLUGIN_PREFIX="${PLUGIN_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_DESKTOP_PREFIX="${DESKTOP_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_TRANSLATION_PREFIX="${TRANSLATION_INSTALL_PREFIX}" )
endif()

add_subdirectory(src)

if (WITH_PLUGINS)
    add_subdirectory(plugins)
endif()

