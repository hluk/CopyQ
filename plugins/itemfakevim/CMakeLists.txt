file(GLOB copyq_plugin_itemfakevim_SOURCES
    ../../src/item/itemwidgetwrapper.cpp
    )
file(GLOB copyq_plugin_itemfakevim_external_SOURCES
    fakevim/*.cpp
    fakevim/utils/*.cpp
    )

set(copyq_plugin_itemfakevim_DEFINITIONS
    QT_NO_CAST_TO_ASCII
    QT_RESTRICTED_CAST_FROM_ASCII
    QTCREATOR_UTILS_STATIC_LIB)

set(copyq_plugin_itemfakevim_RESOURCES itemfakevim.qrc)

copyq_add_plugin(itemfakevim)
target_include_directories(itemfakevim SYSTEM PRIVATE fakevim)
