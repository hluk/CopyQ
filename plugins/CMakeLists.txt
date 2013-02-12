macro (copyq_add_plugin)
    set(copyq_pkg ${ARGV0})

    file(GLOB plugin_${copyq_pkg}_SOURCES *.cpp)
    file(GLOB plugin_${copyq_pkg}_MOCABLE *.h)

    include_directories(${CMAKE_CURRENT_BINARY_DIR} ../../src/include)

    if (WITH_QT5)
        include_directories(${Qt5Widgets_INCLUDES})
        add_definitions(${Qt5Widgets_DEFINITIONS})
        qt5_wrap_cpp(plugin_${copyq_pkg}_MOC ${plugin_${copyq_pkg}_MOCABLE})
        qt5_use_modules(${copyq_pkg} ${plugin_${copyq_pkg}_Qt5_Modules})
    else()
        include_directories(${QT_INCLUDES})
        add_definitions(${QT_DEFINITIONS})
        qt4_wrap_cpp(plugin_${copyq_pkg}_MOC ${plugin_${copyq_pkg}_MOCABLE})
        include(${QT_USE_FILE})
    endif()

    add_library(${copyq_pkg} SHARED ${plugin_${copyq_pkg}_SOURCES} ${plugin_${copyq_pkg}_MOC})

    target_link_libraries(${copyq_pkg} ${QT_LIBRARIES})

    install(TARGETS ${copyq_pkg} DESTINATION ${COPYQ_PLUGIN_INSTALL_DIR})
endmacro()

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/plugins)

add_subdirectory("itemtext")
add_subdirectory("itemimage")
add_subdirectory("itemweb")

