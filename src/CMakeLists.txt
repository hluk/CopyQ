# Project files
file(GLOB copyq_SOURCES
    main.cpp
    app/*.cpp
    common/*.cpp
    gui/*.cpp
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

if (NOT APPLE)
    add_executable(${COPYQ_EXECUTABLE_NAME} ${copyq_COMPILE})
else()
    # On Macs we must ensure Carbon and Cocoa are linked
    FIND_LIBRARY(CARBON_LIBRARY "Carbon")
    FIND_LIBRARY(COCOA_LIBRARY "Cocoa")
    MARK_AS_ADVANCED (CARBON_LIBRARY
        COCOA_LIBRARY)
    list(APPEND copyq_LIBRARIES "${CARBON_LIBRARY}" "${COCOA_LIBRARY}")

    set(COPYQ_VERSION_REGEX "^v([0-9]+)\.([0-9]+)\.(.*)$")
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

add_dependencies(${COPYQ_EXECUTABLE_NAME} generate_version_header)

# Qt modules
list(APPEND copyq_qt_modules Network Svg Xml Qml)
foreach (copyq_qt_module ${copyq_qt_modules})
    find_package(Qt5${copyq_qt_module} REQUIRED)
    list(APPEND copyq_LIBRARIES Qt5::${copyq_qt_module})
endforeach()

# macros definitions for preprocessor and moc
set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES COMPILE_DEFINITIONS "${copyq_DEFINITIONS}")

# link
set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES LINK_FLAGS "${copyq_LINK_FLAGS}")
target_link_libraries(${COPYQ_EXECUTABLE_NAME} ${copyq_LIBRARIES})

# install
install(TARGETS ${COPYQ_EXECUTABLE_NAME}
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
    )

if (NOT APPLE)
    if (TRANSLATION_INSTALL_PREFIX)
        install(FILES ${copyq_QM} DESTINATION "${TRANSLATION_INSTALL_PREFIX}")
    endif()
else()
    set_source_files_properties(${copyq_QM} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/translations")

    set_target_properties(${COPYQ_EXECUTABLE_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../shared/Info.plist")

    set(plugin_dest_dir "${COPYQ_EXECUTABLE_NAME}.app/Contents/PlugIns")
    set(qtconf_dest_dir "${COPYQ_EXECUTABLE_NAME}.app/Contents/Resources")

    macro(install_qt5_plugin _qt_plugin_name _qt_plugins_var)
        get_target_property(_qt_plugin_path "${_qt_plugin_name}" LOCATION)
        if(EXISTS "${_qt_plugin_path}")
            get_filename_component(_qt_plugin_file "${_qt_plugin_path}" NAME)
            get_filename_component(_qt_plugin_type "${_qt_plugin_path}" PATH)
            get_filename_component(_qt_plugin_type "${_qt_plugin_type}" NAME)
            set(_qt_plugin_dest "${plugin_dest_dir}/${_qt_plugin_type}")
            install(FILES "${_qt_plugin_path}" DESTINATION "${_qt_plugin_dest}" COMPONENT Runtime)
            set(${_qt_plugins_var}
                "${${_qt_plugins_var}};\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_qt_plugin_dest}/${_qt_plugin_file}")
        else()
            message(FATAL_ERROR "QT plugin ${_qt_plugin_name} not found")
        endif()
    endmacro()

    # Install needed Qt plugins
    foreach(_copyq_qt_module ${copyq_qt_modules} "Gui" "Widgets")
        set(_module_plugins "${Qt5${_copyq_qt_module}_PLUGINS}")
        foreach(_plugin ${_module_plugins})
            install_qt5_plugin("${_plugin}" FIXUP_BUNDLE_QT_PLUGINS)
        endforeach()
    endforeach()

    # install a qt.conf file
    # this inserts some cmake code into the install script to write the file
    install(CODE "
        file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${qtconf_dest_dir}/qt.conf\" \"[Paths]\nPlugins = PlugIns\nImports = Resources/qml\nQml2Imports = Resources/qml\n\")
        "
        COMPONENT Runtime
        )

    set(FIXUP_BUNDLE_APPS "\${CMAKE_INSTALL_PREFIX}/${COPYQ_EXECUTABLE_NAME}.app")

    get_property(_copyq_installed_plugins GLOBAL PROPERTY COPYQ_INSTALLED_PLUGINS)

    # Directories to look for dependencies
    set(FIXUP_BUNDLE_DEP_DIRS "${CMAKE_BINARY_DIR};${QT_LIBRARY_DIRS}")

    install(CODE "
        include(BundleUtilities)
        set(BU_CHMOD_BUNDLE_ITEMS ON)
        fixup_bundle(\"${FIXUP_BUNDLE_APPS}\" \"${FIXUP_BUNDLE_QT_PLUGINS};${_copyq_installed_plugins}\" \"${FIXUP_BUNDLE_DEP_DIRS}\")
        verify_app(\"${FIXUP_BUNDLE_APPS}\")
        "
        COMPONENT Runtime
        )

    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()
