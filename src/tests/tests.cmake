message(STATUS "Building with tests.")

file(GLOB copyq_tests_SOURCES tests/*.cpp)
add_executable(copyq-tests
    ${copyq_tests_SOURCES}
    $<TARGET_OBJECTS:copyq-common>
)
set_target_properties(copyq-tests PROPERTIES COMPILE_DEFINITIONS "${copyq_DEFINITIONS}")

find_package(${copyq_qt}Test REQUIRED)
target_link_libraries(copyq-tests ${copyq_LIBRARIES} ${copyq_qt}::Test)
target_include_directories(copyq-tests PRIVATE .)

set(copyq_pkg itemtests)
set(copyq_plugin_SOURCES
    tests/itemtests/itemtests.cpp
    item/itemwidget.cpp
    )
add_library(${copyq_pkg} MODULE ${copyq_plugin_SOURCES})
target_link_libraries(${copyq_pkg} ${copyq_qt}::Widgets ${copyq_qt}::Test)
target_include_directories(${copyq_pkg} PRIVATE .)
