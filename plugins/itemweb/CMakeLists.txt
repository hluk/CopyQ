OPTION(WITH_WEBKIT "WebKit support" ON)

# Qt modules
if (WITH_QT5)
    # webkit
    if (WITH_WEBKIT)
        find_package(Qt5WebKitWidgets QUIET)
        set(HAS_WEBKIT ${Qt5WebKitWidgets_FOUND})
    endif(WITH_WEBKIT)
else()
    # webkit
    if (WITH_WEBKIT)
        set(HAS_WEBKIT ${QT_QTWEBKIT_FOUND})
    endif(WITH_WEBKIT)
endif()

# webkit
if (HAS_WEBKIT)
    message(STATUS "Building with ItemWeb plugin.")

    if (WITH_QT5)
        set(copyq_plugin_itemweb_Qt5_Modules WebKitWidgets)
    else()
        set(QT_USE_QTWEBKIT ON)
    endif()

    set(copyq_plugin_itemweb_SOURCES
        ../../src/common/mimetypes.cpp
        ../../src/common/textdata.cpp
        )

    copyq_add_plugin(itemweb)
endif(HAS_WEBKIT)

