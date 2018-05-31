# Project files
file(GLOB copyq_SOURCES
    main.cpp
    app/*.cpp
    common/*.cpp
    gui/*.cpp
    item/*.cpp
    scriptable/*.cpp
    scriptable/scriptableproxy.h
    ../qt/*.cpp
    )

file(GLOB copyq_FORMS
    ui/*.ui
    )

set(copyq_RESOURCES copyq.qrc)

# Qt include paths and definitions
include_directories(${CMAKE_CURRENT_BINARY_DIR} .)
include_directories(${Qt5Widgets_INCLUDES})
add_definitions(${Qt5Widgets_DEFINITIONS})

include(platform/platform.cmake)

# Qxt - global shortcuts
if (USE_QXT)
    set(copyq_SOURCES ${copyq_SOURCES}
        ../qxt/qxtglobalshortcut.cpp
        )
    add_definitions( -DQXT_STATIC )
    include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
endif()

# Compile with tests?
if (WITH_TESTS)
    file(GLOB copyq_SOURCES ${copyq_SOURCES} tests/*.cpp)
endif()

# translations
file(GLOB copyq_TRANSLATIONS ../translations/*.ts)

find_package(Qt5LinguistTools)
if (Qt5LinguistTools_FOUND)
    qt5_add_translation(copyq_QM ${copyq_TRANSLATIONS})
endif()

qt5_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
qt5_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

set(copyq_COMPILE
    ${copyq_COMPILE}
    ${copyq_SOURCES}
    ${copyq_FORMS_HEADERS}
    ${copyq_RESOURCES_RCC}
    ${copyq_QM}
    )

add_executable(copyq ${copyq_COMPILE})

if (APPLE)
    # On Macs we must ensure Carbon and Cocoa are linked
    #
    # We do this using a macro taken from:
    # http://stackoverflow.com/questions/12547624/cant-link-macos-frameworks-with-cmake?rq=1
    macro(ADD_FRAMEWORK fwname appname)
        find_library(FRAMEWORK_${fwname}
            NAMES ${fwname}
            PATHS ${CMAKE_OSX_SYSROOT}/System/Library
            PATH_SUFFIXES Frameworks
            NO_DEFAULT_PATH)
        if( ${FRAMEWORK_${fwname}} STREQUAL FRAMEWORK_${fwname}-NOTFOUND)
            MESSAGE(FATAL_ERROR ": Framework ${fwname} not found")
        else()
            TARGET_LINK_LIBRARIES(${appname} "${FRAMEWORK_${fwname}}/${fwname}")
            MESSAGE(STATUS "Framework ${fwname} found at ${FRAMEWORK_${fwname}}")
        endif()
    endmacro(ADD_FRAMEWORK)

    ADD_FRAMEWORK(Carbon copyq)
    ADD_FRAMEWORK(Cocoa copyq)
endif()

# Qt modules
list(APPEND copyq_qt_modules Network Svg Xml Script)
foreach (copyq_qt_module ${copyq_qt_modules})
    find_package(Qt5${copyq_qt_module} REQUIRED)
    list(APPEND copyq_LIBRARIES Qt5::${copyq_qt_module})
endforeach()

# macros definitions for preprocessor and moc
set_target_properties(copyq PROPERTIES COMPILE_DEFINITIONS "${copyq_DEFINITIONS}")

# link
set_target_properties(copyq PROPERTIES LINK_FLAGS "${copyq_LINK_FLAGS}")
target_link_libraries(copyq ${copyq_LIBRARIES})

# install
install(TARGETS copyq DESTINATION bin)

if (TRANSLATION_INSTALL_PREFIX)
    install(FILES ${copyq_QM} DESTINATION "${TRANSLATION_INSTALL_PREFIX}")
endif()
