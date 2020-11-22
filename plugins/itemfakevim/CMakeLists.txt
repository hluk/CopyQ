file(GLOB copyq_plugin_itemfakevim_SOURCES
    ../../src/item/itemwidgetwrapper.cpp
    )
file(GLOB copyq_plugin_itemfakevim_external_SOURCES
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
