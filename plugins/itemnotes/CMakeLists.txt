set(copyq_plugin_itemnotes_SOURCES
    $<TARGET_OBJECTS:copyq-plugin-iconfont>
    $<TARGET_OBJECTS:copyq-plugin-iconwidget>
    $<TARGET_OBJECTS:copyq-plugin-textdata>
    $<TARGET_OBJECTS:copyq-plugin-wrapper>
    )

copyq_add_plugin(itemnotes)
