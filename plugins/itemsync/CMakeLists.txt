OPTION(COPYQ_ITEMSYNC_UPDATE_INTERVAL_MS "Interval in ms for updating items from files" -1)
if (COPYQ_ITEMSYNC_UPDATE_INTERVAL_MS GREATER 0)
    add_definitions( -DCOPYQ_ITEMSYNC_UPDATE_INTERVAL_MS=${COPYQ_ITEMSYNC_UPDATE_INTERVAL_MS}  )
endif()

set(copyq_plugin_itemsync_SOURCES
    ../../src/common/common.cpp
    ../../src/common/config.cpp
    ../../src/common/log.cpp
    ../../src/common/mimetypes.cpp
    ../../src/gui/iconfont.cpp
    ../../src/gui/iconselectbutton.cpp
    ../../src/gui/iconselectdialog.cpp
    ../../src/gui/iconwidget.cpp
    ../../src/item/serialize.cpp
    )

copyq_add_plugin(itemsync)

