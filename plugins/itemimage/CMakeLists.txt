set(copyq_plugin_itemimage_SOURCES
    $<TARGET_OBJECTS:copyq-plugin-serialize>
    ../../src/common/action.cpp
    ../../src/common/process.cpp
    ../../src/common/temporaryfile.cpp
    ../../src/item/itemeditor.cpp
    )

copyq_add_plugin(itemimage)
