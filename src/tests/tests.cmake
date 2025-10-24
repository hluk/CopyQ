message(STATUS "Building with tests.")

list(FILTER copyq_SOURCES INCLUDE REGEX "platform/")
file(GLOB copyq_SOURCES
    ${copyq_SOURCES}
    tests/*.cpp
    app/applicationexceptionhandler.cpp
    common/appconfig.cpp
    common/clipboarddataguard.cpp
    common/common.cpp
    common/config.cpp
    common/display.cpp
    common/log.cpp
    common/mimetypes.cpp
    common/process.cpp
    common/settings.cpp
    common/shortcuts.cpp
    common/textdata.cpp
    gui/clipboardspy.cpp
    gui/screen.cpp
)

add_executable(copyq-tests ${copyq_SOURCES})
target_sources(copyq-tests PRIVATE ${copyq_SOURCES})
set_target_properties(copyq-tests PROPERTIES COMPILE_DEFINITIONS "${copyq_DEFINITIONS}")

find_package(${copyq_qt}Test REQUIRED)
target_link_libraries(copyq-tests ${copyq_LIBRARIES} ${copyq_qt}::Test)
target_include_directories(copyq-tests PRIVATE . tests)
add_dependencies(copyq-tests generate_version_file)

set(copyq_pkg itemtests)
set(copyq_plugin_SOURCES
    tests/itemtests/itemtests.cpp
    item/itemwidget.cpp
    )
add_definitions(${${copyq_qt}Widgets_DEFINITIONS})
add_library(${copyq_pkg} MODULE ${copyq_plugin_SOURCES})
target_link_libraries(${copyq_pkg} ${copyq_qt}::Widgets ${copyq_qt}::Test)
target_include_directories(${copyq_pkg} PRIVATE .)
