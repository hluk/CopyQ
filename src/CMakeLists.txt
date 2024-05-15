# Project files
file(GLOB copyq_SOURCES
    main.cpp
    app/*.cpp
    common/*.cpp
    gui/*.cpp
    gui/notification.h
    item/*.cpp
    scriptable/*.cpp
    scriptable/scriptableproxy.h
    )

file(GLOB copyq_FORMS
    ui/*.ui
    )

set(copyq_RESOURCES copyq.qrc)

# Qt include paths and definitions
include_directories(${CMAKE_CURRENT_BINARY_DIR} .)
include_directories(${${copyq_qt}Widgets_INCLUDES})
add_definitions(${${copyq_qt}Widgets_DEFINITIONS})

include(notifications.cmake)
include(platform/platform.cmake)

# Compile with tests?
if (WITH_TESTS)
    file(GLOB copyq_SOURCES ${copyq_SOURCES} tests/*.cpp)
endif()

set_source_files_properties(
    ${copyq_SOURCES} PROPERTIES
    COMPILE_OPTIONS "${copyq_pedantic_flags}")

# Qxt - global shortcuts
if (USE_QXT)
    set(copyq_SOURCES ${copyq_SOURCES}
        ../qxt/qxtglobalshortcut.cpp
        )
    add_definitions( -DQXT_STATIC )
    add_definitions( -DCOPYQ_GLOBAL_SHORTCUTS )
endif()

# translations
file(GLOB copyq_TRANSLATIONS ../translations/*.ts)

find_package(${copyq_qt}LinguistTools)
if (${copyq_qt}LinguistTools_FOUND)
    qt_add_translation(copyq_QM ${copyq_TRANSLATIONS})
endif()

qt_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
qt_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

# Generate version.h from current git commit if available.
set(copyq_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/common/version.cpp")
add_custom_target(
    generate_version_file
    ALL
    COMMAND ${CMAKE_COMMAND}
        -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/common/version.cpp.in
        -D OUTPUT_FILE=${copyq_VERSION_FILE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/version_file.cmake
    BYPRODUCTS ${copyq_VERSION_FILE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_source_files_properties(${copyq_VERSION_FILE} PROPERTIES GENERATED TRUE)
set_property(SOURCE ${copyq_VERSION_FILE} PROPERTY SKIP_AUTOMOC ON)

set(copyq_COMPILE
    ${copyq_COMPILE}
    ${copyq_SOURCES}
    ${copyq_FORMS_HEADERS}
    ${copyq_RESOURCES_RCC}
    ${copyq_QM}
    ${copyq_VERSION_FILE}
    )

if (NOT copyq_version)
    message(FATAL_ERROR "Application version is unset")
endif()

if (NOT APPLE)
    add_executable(${COPYQ_EXECUTABLE_NAME} ${copyq_COMPILE})
else()
    FIND_LIBRARY(CARBON_LIBRARY Carbon REQUIRED)
    FIND_LIBRARY(COCOA_LIBRARY Cocoa REQUIRED)
    MARK_AS_ADVANCED(CARBON_LIBRARY COCOA_LIBRARY)
    list(APPEND copyq_LIBRARIES "${CARBON_LIBRARY}" "${COCOA_LIBRARY}")

    set(COPYQ_VERSION_REGEX "^([0-9]+)\.([0-9]+)\.(.*)$")
    string(REGEX REPLACE "${COPYQ_VERSION_REGEX}"
        "\\1" CPACK_PACKAGE_VERSION_MAJOR
        "${copyq_version}"
        )
    string(REGEX REPLACE "${COPYQ_VERSION_REGEX}"
        "\\2" CPACK_PACKAGE_VERSION_MINOR
        "${copyq_version}"
        )
    string(REGEX REPLACE "${COPYQ_VERSION_REGEX}"
        "\\3" CPACK_PACKAGE_VERSION_PATCH
        "${copyq_version}"
        )

    set(MACOSX_BUNDLE_BUNDLE_NAME "${COPYQ_EXECUTABLE_NAME}")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.hluk.CopyQ")
    set(MACOSX_BUNDLE_INFO_STRING "Advanced clipboard manager with editing and scripting features")
    set(MACOSX_BUNDLE_ICON_FILE "icon.icns")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${copyq_version}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${copyq_version}")

    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/images/icon.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set(copyq_COMPILE ${copyq_COMPILE} "${CMAKE_CURRENT_SOURCE_DIR}/images/icon.icns")

    add_executable(${COPYQ_EXECUTABLE_NAME} MACOSX_BUNDLE ${copyq_COMPILE})
endif()

target_sources(${COPYQ_EXECUTABLE_NAME} PRIVATE ${copyq_SOURCES})
add_dependencies(${COPYQ_EXECUTABLE_NAME} generate_version_file)

# Qt modules
list(APPEND copyq_qt_modules Network Svg Xml Qml)
foreach (copyq_qt_module ${copyq_qt_modules})
    find_package(${copyq_qt}${copyq_qt_module} REQUIRED)
    list(APPEND copyq_LIBRARIES ${copyq_qt}::${copyq_qt_module})
endforeach()

# macros definitions for preprocessor and moc
set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES COMPILE_DEFINITIONS "${copyq_DEFINITIONS}")

# link
set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES LINK_FLAGS "${copyq_LINK_FLAGS}")
target_link_libraries(${COPYQ_EXECUTABLE_NAME} ${copyq_LIBRARIES})

# install
if (WIN32)
    install(TARGETS ${COPYQ_EXECUTABLE_NAME}
        RUNTIME DESTINATION . COMPONENT Runtime
        LIBRARY DESTINATION . COMPONENT Runtime
        )
else()
    install(TARGETS ${COPYQ_EXECUTABLE_NAME}
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION bin COMPONENT Runtime
        )
endif()

if (NOT APPLE)
    if (TRANSLATION_INSTALL_PREFIX)
        install(FILES ${copyq_QM} DESTINATION "${TRANSLATION_INSTALL_PREFIX}")
    endif()
else()
    set_source_files_properties(${copyq_QM} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/translations")

    set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
    set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../shared/Info.plist")

    set(BUNDLE "\${CMAKE_INSTALL_PREFIX}/${COPYQ_EXECUTABLE_NAME}.app")

    # https://doc.qt.io/qt-6/qt-deploy-runtime-dependencies.html
    # Generate a deployment script to be executed at install time
    qt_generate_deploy_script(
        TARGET ${COPYQ_EXECUTABLE_NAME}
        OUTPUT_SCRIPT deploy_script
        CONTENT "
qt_deploy_runtime_dependencies(
    EXECUTABLE \"${BUNDLE}\"
    GENERATE_QT_CONF
    VERBOSE
)")
    install(SCRIPT ${deploy_script})

    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()
