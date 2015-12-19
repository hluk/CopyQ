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
if (WITH_QT5)
    include_directories(${Qt5Widgets_INCLUDES})
    add_definitions(${Qt5Widgets_DEFINITIONS})
else()
    include_directories(${QT_INCLUDES})
    add_definitions(${QT_DEFINITIONS})
endif()

include(platform/platform.cmake)

# Qxt - global shortcuts
if (USE_QXT)
    set(copyq_SOURCES ${copyq_SOURCES}
        ../qxt/qxtglobal.cpp
        ../qxt/qxtglobalshortcut.cpp
        )
    add_definitions( -DQXT_STATIC )
endif()

# Compile with tests?
if (WITH_TESTS)
    file(GLOB copyq_SOURCES ${copyq_SOURCES} tests/*.cpp)
endif()

# translations
file(GLOB copyq_TRANSLATIONS ../translations/*.ts)

if (WITH_QT5)
    find_package(Qt5LinguistTools)
    if (Qt5LinguistTools_FOUND)
        qt5_add_translation(copyq_QM ${copyq_TRANSLATIONS})
    endif()
else()
    qt4_add_translation(copyq_QM ${copyq_TRANSLATIONS})
endif()

if (WITH_QT5)
    qt5_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
    qt5_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES})
else()
    qt4_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
    qt4_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES})
endif()

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
            MESSAGE(ERROR ": Framework ${fwname} not found")
        else()
            TARGET_LINK_LIBRARIES(${appname} "${FRAMEWORK_${fwname}}/${fwname}")
            MESSAGE(STATUS "Framework ${fwname} found at ${FRAMEWORK_${fwname}}")
        endif()
    endmacro(ADD_FRAMEWORK)

    ADD_FRAMEWORK(Carbon copyq)
    ADD_FRAMEWORK(Cocoa copyq)
endif()

# Qt modules
if (WITH_QT5)
    qt5_use_modules(copyq Widgets Network Svg Xml Script ${copyq_Qt5_Modules})
else()
    set(QT_USE_QTNETWORK TRUE)
    set(QT_USE_QTSVG TRUE)
    set(QT_USE_QTXML TRUE)
    set(QT_USE_QTSCRIPT TRUE)
    include(${QT_USE_FILE})
endif()

# macros definitions for preprocessor and moc
set_target_properties(copyq PROPERTIES COMPILE_DEFINITIONS "${copyq_DEFINITIONS}")

# link
set_target_properties(copyq PROPERTIES LINK_FLAGS "${copyq_LINK_FLAGS}")
target_link_libraries(copyq ${QT_LIBRARIES} ${copyq_LIBRARIES})

# install
install(TARGETS copyq DESTINATION bin)

if (TRANSLATION_INSTALL_PREFIX)
    install(FILES ${copyq_QM} DESTINATION "${TRANSLATION_INSTALL_PREFIX}")
endif()
