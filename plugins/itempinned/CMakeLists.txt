set(copyq_plugin_itempinned_SOURCES
    $<TARGET_OBJECTS:copyq-plugin-wrapper>
    ../../src/item/itemsaverwrapper.cpp
    ../../src/common/display.cpp
    $<TARGET_OBJECTS:copyq-plugin-screen>
    )

copyq_add_plugin(itempinned)
