OPTION(WITH_WEBKIT "WebKit support" ON)

# Qt modules
if (WITH_WEBKIT)
    find_package(Qt5WebKitWidgets QUIET)
    set(HAS_WEBKIT ${Qt5WebKitWidgets_FOUND})
endif(WITH_WEBKIT)

# webkit
if (HAS_WEBKIT)
    message(STATUS "Building with ItemWeb plugin.")

    set(copyq_plugin_itemweb_LIBRARIES Qt5::WebKitWidgets)

    set(copyq_plugin_itemweb_SOURCES
        ../../src/common/mimetypes.cpp
        ../../src/common/textdata.cpp
        )

    copyq_add_plugin(itemweb)
endif(HAS_WEBKIT)

