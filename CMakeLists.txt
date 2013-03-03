cmake_minimum_required(VERSION 2.8)
project(copyq)

OPTION(WITH_QT5 "Qt5 support" OFF)
OPTION(WITH_TESTS "Run test cases from command line" OFF)
OPTION(WITH_PLUGINS "Compile plugins" ON)

# If Qt4 unavailable use Qt5.
if (NOT WITH_QT5)
    find_package(Qt4)
    if (NOT QT4_FOUND)
        set(WITH_QT5 ON)
        message(STATUS "Qt 4 is unavailable trying Qt 5.")
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
    set(WITH_TESTS ON)
    add_definitions( -DCOPYQ_LOG_DEBUG )
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(copyq_ICON_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
    add_definitions( -DCOPYQ_ICON_PREFIX="${copyq_ICON_INSTALL_PREFIX}/copyq" )
    install(FILES ${copyq_ICON}        DESTINATION ${copyq_ICON_INSTALL_PREFIX} RENAME copyq.svg)
    install(FILES ${copyq_ICON_NORMAL} DESTINATION ${copyq_ICON_INSTALL_PREFIX} RENAME copyq-normal.svg)
    install(FILES ${copyq_ICON_BUSY}   DESTINATION ${copyq_ICON_INSTALL_PREFIX} RENAME copyq-busy.svg)
    install(FILES ${copyq_DESKTOP}     DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    set(copyq_plugin_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/copyq/plugins")
else()
    set(copyq_plugin_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/plugins")
endif()

add_subdirectory(src)

if (WITH_PLUGINS)
    add_subdirectory(plugins)
endif()

