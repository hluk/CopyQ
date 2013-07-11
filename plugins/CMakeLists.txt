macro (copyq_add_plugin)
    set(copyq_pkg ${ARGV0})

    file(GLOB copyq_plugin_${copyq_pkg}_SOURCES
        ${copyq_plugin_${copyq_pkg}_SOURCES}
        *.cpp
        ../../src/item/itemwidget.cpp
        )
    file(GLOB copyq_plugin_${copyq_pkg}_MOCABLE
        ${copyq_plugin_${copyq_pkg}_MOCABLE}
        *.h
        )
    file(GLOB copyq_plugin_${copyq_pkg}_FORMS
        ${copyq_plugin_${copyq_pkg}_FORMS}
        *.ui
        )

    include_directories(${CMAKE_CURRENT_BINARY_DIR} ../../src)

    if (WITH_QT5)
        include_directories(${Qt5Widgets_INCLUDES})
        add_definitions(${Qt5Widgets_DEFINITIONS})
        qt5_wrap_cpp(copyq_plugin_${copyq_pkg}_MOC ${copyq_plugin_${copyq_pkg}_MOCABLE})
        qt5_wrap_ui(copyq_plugin_${copyq_pkg}_FORMS_HEADERS ${copyq_plugin_${copyq_pkg}_FORMS})
    else()
        include_directories(${QT_INCLUDES})
        add_definitions(${QT_DEFINITIONS})
        qt4_wrap_cpp(copyq_plugin_${copyq_pkg}_MOC ${copyq_plugin_${copyq_pkg}_MOCABLE})
        qt4_wrap_ui(copyq_plugin_${copyq_pkg}_FORMS_HEADERS ${copyq_plugin_${copyq_pkg}_FORMS})
    endif()

    add_library(${copyq_pkg} SHARED
        ${copyq_plugin_${copyq_pkg}_SOURCES}
        ${copyq_plugin_${copyq_pkg}_MOC}
        ${copyq_plugin_${copyq_pkg}_FORMS_HEADERS}
        )

    if (WITH_QT5)
        qt5_use_modules(${copyq_pkg} Widgets ${copyq_plugin_${copyq_pkg}_Qt5_Modules})
    else()
        include(${QT_USE_FILE})
    endif()

    target_link_libraries(${copyq_pkg} ${QT_LIBRARIES})

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        install(TARGETS ${copyq_pkg} DESTINATION ${PLUGIN_INSTALL_PREFIX})
    endif()
endmacro()

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/plugins)

add_subdirectory("itemtext")
add_subdirectory("itemimage")
add_subdirectory("itemweb")
add_subdirectory("itemdata")
