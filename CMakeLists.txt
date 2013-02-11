project(copyq)

OPTION(WITH_QT5 "Qt5 support" OFF)
OPTION(WITH_TESTS "Run test cases from command line" OFF)
OPTION(WITH_PLUGINS "Compile plugins" ON)

if (WITH_QT5)
    cmake_minimum_required(VERSION 2.8.8)
    find_package(Qt5Widgets REQUIRED)
else()
    cmake_minimum_required(VERSION 2.8)
    find_package(Qt4 REQUIRED)
endif()

set(copyq_ICON src/images/logo.svg)
set(copyq_DESKTOP shared/copyq.desktop)

# Be more strict while compiling debugging version
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra -Wall -pedantic")
endif()

# Tests always on with debug version
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(WITH_TESTS ON)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(FILES ${copyq_ICON} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons" RENAME copyq.svg)
    install(FILES ${copyq_DESKTOP} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    set(COPYQ_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/copyq/plugins")
else()
    set(COPYQ_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/plugins")
endif()

add_subdirectory(src)

if (WITH_PLUGINS)
    add_subdirectory(plugins)
endif()

