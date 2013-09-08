file(GLOB copyq_plugin_itemfakevim_SOURCES
    ${copyq_plugin_itemfakevim_SOURCES}
    fakevim/*.cpp
    fakevim/utils/*.cpp
    )

include_directories(fakevim)

add_definitions( -DFAKEVIM_STANDALONE )

set(copyq_plugin_itemfakevim_RESOURCES itemfakevim.qrc)

copyq_add_plugin(itemfakevim)

