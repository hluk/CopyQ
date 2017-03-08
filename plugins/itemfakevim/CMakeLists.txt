file(GLOB copyq_plugin_itemfakevim_SOURCES
    ${copyq_plugin_itemfakevim_SOURCES}
    fakevim/*.cpp
    fakevim/utils/*.cpp
    )

include_directories(fakevim)

add_definitions( -DFAKEVIM_STANDALONE -DQTCREATOR_UTILS_STATIC_LIB )
set(copyq_plugin_itemfakevim_DEFINITIONS
    FAKEVIM_STANDALONE
    QTCREATOR_UTILS_STATIC_LIB)

set(copyq_plugin_itemfakevim_RESOURCES itemfakevim.qrc)

copyq_add_plugin(itemfakevim)

# Disable warnings for 3rd-party source code.
if (PEDANTIC)
    if (CMAKE_COMPILER_IS_GNUCXX)
        set(IGNORE_PEDANTIC_FLAGS "-Wno-suggest-override")
    else()
        set(IGNORE_PEDANTIC_FLAGS "-Wno-unused-macros")
    endif()

    set_source_files_properties(
        fakevim/fakevimhandler.cpp
        fakevim/fakevimactions.cpp
        PROPERTIES COMPILE_FLAGS
        "\
        -Wno-shorten-64-to-32 \
        -Wno-sign-conversion \
        -Wno-conversion \
        -Wno-unreachable-code \
        -Wno-documentation-unknown-command \
        -Wno-shadow \
        -Wno-missing-declarations \
        -Wno-strict-overflow \
        ${IGNORE_PEDANTIC_FLAGS} \
        ")
endif()

