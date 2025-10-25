set(copyq_plugin_itemencrypted_SOURCES
    $<TARGET_OBJECTS:copyq-plugin-config>
    $<TARGET_OBJECTS:copyq-plugin-iconfont>
    $<TARGET_OBJECTS:copyq-plugin-iconwidget>
    $<TARGET_OBJECTS:copyq-plugin-screen>
    $<TARGET_OBJECTS:copyq-plugin-serialize>
    $<TARGET_OBJECTS:copyq-plugin-textdata>
    ../../src/common/shortcuts.cpp
    )

copyq_add_plugin(itemencrypted)
