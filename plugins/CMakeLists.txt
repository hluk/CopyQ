macro (copyq_add_plugin)
    set(copyq_pkg ${ARGV0})

    file(GLOB copyq_plugin_SOURCES
        ${copyq_plugin_${copyq_pkg}_SOURCES}
        *.cpp
        ../../src/item/itemwidget.cpp
        )
    file(GLOB copyq_plugin_FORMS
        ${copyq_plugin_${copyq_pkg}_FORMS}
        *.ui
        )

    include_directories(${CMAKE_CURRENT_BINARY_DIR} ../../src)

    if (WITH_QT5)
        include_directories(${Qt5Widgets_INCLUDES})
        add_definitions(${Qt5Widgets_DEFINITIONS})
        qt5_wrap_ui(copyq_plugin_FORMS_HEADERS ${copyq_plugin_FORMS})
        qt5_add_resources(copyq_plugin_RCC ${copyq_plugin_${copyq_pkg}_RESOURCES})
    else()
        include_directories(${QT_INCLUDES})
        add_definitions(${QT_DEFINITIONS})
        qt4_wrap_ui(copyq_plugin_FORMS_HEADERS ${copyq_plugin_FORMS})
        qt4_add_resources(copyq_plugin_RCC ${copyq_plugin_${copyq_pkg}_RESOURCES})
    endif()

    add_library(${copyq_pkg} SHARED
        ${copyq_plugin_SOURCES}
        ${copyq_plugin_FORMS_HEADERS}
        ${copyq_plugin_RCC}
        )

    if (WITH_QT5)
        qt5_use_modules(${copyq_pkg} Widgets ${copyq_plugin_${copyq_pkg}_Qt5_Modules})
    else()
        include(${QT_USE_FILE})
    endif()

    target_link_libraries(${copyq_pkg} ${QT_LIBRARIES} ${copyq_plugin_${copyq_pkg}_LIBRARIES})

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        install(TARGETS ${copyq_pkg} DESTINATION ${PLUGIN_INSTALL_PREFIX})
    endif()
endmacro()

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/plugins)

add_subdirectory("itemdata")
add_subdirectory("itemencrypted")
add_subdirectory("itemfakevim")
add_subdirectory("itemimage")
add_subdirectory("itemnotes")
add_subdirectory("itemtext")
add_subdirectory("itemweb")
