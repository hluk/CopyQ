set(copyq_plugin_itemtags_SOURCES
    $<TARGET_OBJECTS:copyq-plugin-config>
    $<TARGET_OBJECTS:copyq-plugin-iconfont>
    $<TARGET_OBJECTS:copyq-plugin-iconwidget>
    $<TARGET_OBJECTS:copyq-plugin-screen>
    $<TARGET_OBJECTS:copyq-plugin-textdata>
    $<TARGET_OBJECTS:copyq-plugin-wrapper>
    ../../src/gui/iconselectbutton.cpp
    ../../src/gui/iconselectdialog.cpp
    ../../src/item/itemsaverwrapper.cpp
    )

copyq_add_plugin(itemtags)
