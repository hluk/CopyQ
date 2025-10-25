set(copyq_plugin_itemsync_SOURCES
    $<TARGET_OBJECTS:copyq-plugin-config>
    $<TARGET_OBJECTS:copyq-plugin-iconfont>
    $<TARGET_OBJECTS:copyq-plugin-iconwidget>
    $<TARGET_OBJECTS:copyq-plugin-screen>
    $<TARGET_OBJECTS:copyq-plugin-serialize>
    $<TARGET_OBJECTS:copyq-plugin-wrapper>
    ../../src/gui/iconselectbutton.cpp
    ../../src/gui/iconselectdialog.cpp
    )

copyq_add_plugin(itemsync)
