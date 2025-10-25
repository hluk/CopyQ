macro (copyq_add_plugin copyq_pkg)
    file(GLOB copyq_plugin_SOURCES
        ${copyq_plugin_${copyq_pkg}_external_SOURCES}
        *.cpp
        )
    file(GLOB copyq_plugin_FORMS
        ${copyq_plugin_${copyq_pkg}_FORMS}
        *.ui
        )

    set_source_files_properties(
        ${copyq_plugin_${copyq_pkg}_SOURCES} PROPERTIES
        COMPILE_OPTIONS "${copyq_pedantic_flags}")

    qt_wrap_ui(copyq_plugin_FORMS_HEADERS ${copyq_plugin_FORMS})
    qt_add_resources(copyq_plugin_RCC ${copyq_plugin_${copyq_pkg}_RESOURCES})

    add_library(${copyq_pkg} MODULE
        ${copyq_plugin_SOURCES}
        ${copyq_plugin_FORMS_HEADERS}
        ${copyq_plugin_RCC}
        ${copyq_plugin_${copyq_pkg}_SOURCES}
        $<TARGET_OBJECTS:copyq-plugin-common>
        )

    set_target_properties(${copyq_pkg} PROPERTIES
        COMPILE_DEFINITIONS "${copyq_plugin_${copyq_pkg}_DEFINITIONS}")

    target_link_libraries(${copyq_pkg} ${copyq_LIBRARIES} ${copyq_plugin_${copyq_pkg}_LIBRARIES})

    target_include_directories(${copyq_pkg} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ../../src)

    if (UNIX AND NOT APPLE)
        install(TARGETS ${copyq_pkg} DESTINATION ${PLUGIN_INSTALL_PREFIX})
    elseif (APPLE)
        set(copyq_plugins_dest_dir "${COPYQ_EXECUTABLE_NAME}.app/Contents/PlugIns/copyq")
        install(TARGETS ${copyq_pkg} DESTINATION "${copyq_plugins_dest_dir}" COMPONENT Runtime)
        get_property(_copyq_installed_plugins GLOBAL PROPERTY COPYQ_INSTALLED_PLUGINS)
        # We need this list to perform fixups during the creation of an application bundle (in src)
        list(APPEND _copyq_installed_plugins
            "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${copyq_plugins_dest_dir}/${CMAKE_SHARED_MODULE_PREFIX}${copyq_pkg}${CMAKE_SHARED_MODULE_SUFFIX}"
        )
        set_property(GLOBAL PROPERTY COPYQ_INSTALLED_PLUGINS "${_copyq_installed_plugins}")
    endif()
endmacro()

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/plugins)

macro (copyq_add_plugin_obj copyq_mod)
    add_library(${copyq_mod} OBJECT ${ARGN})
    set_target_properties(${copyq_mod} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${copyq_mod} ${copyq_LIBRARIES})
    target_include_directories(${copyq_mod} PRIVATE ../src)
endmacro()

copyq_add_plugin_obj(copyq-plugin-common
    ../src/item/itemwidget.cpp
    ../src/common/mimetypes.cpp
)
copyq_add_plugin_obj(copyq-plugin-wrapper ../src/item/itemwidgetwrapper.cpp)
copyq_add_plugin_obj(copyq-plugin-textdata ../src/common/textdata.cpp)
copyq_add_plugin_obj(copyq-plugin-screen ../src/gui/screen.cpp)
copyq_add_plugin_obj(copyq-plugin-serialize ../src/item/serialize.cpp)
copyq_add_plugin_obj(copyq-plugin-config ../src/common/config.cpp)
copyq_add_plugin_obj(copyq-plugin-iconfont ../src/gui/iconfont.cpp)
copyq_add_plugin_obj(copyq-plugin-iconwidget ../src/gui/iconwidget.cpp)

add_subdirectory("itemencrypted")
add_subdirectory("itemfakevim")
add_subdirectory("itemimage")
add_subdirectory("itemnotes")
add_subdirectory("itempinned")
add_subdirectory("itemtags")
add_subdirectory("itemtext")
add_subdirectory("itemsync")
