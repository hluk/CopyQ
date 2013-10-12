file(GLOB copyq_plugin_itemfakevim_SOURCES
    ${copyq_plugin_itemfakevim_SOURCES}
    fakevim/*.cpp
    fakevim/utils/*.cpp
    )

include_directories(fakevim)

set(copyq_plugin_itemfakevim_DEFINITIONS
    FAKEVIM_STANDALONE
    QTCREATOR_UTILS_STATIC_LIB)

set(copyq_plugin_itemfakevim_RESOURCES itemfakevim.qrc)

copyq_add_plugin(itemfakevim)

