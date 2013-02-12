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

    set(plugin_itemweb_MOCABLE ${plugin_itemweb_MOCABLE}
        src/include/itemweb.h
        )

    if (WITH_QT5)
        qt5_use_modules(copyq WebKitWidgets)
    else()
        set(QT_USE_QTWEBKIT ${HAS_WEBKIT})
        include(${QT_USE_FILE})
    endif()

    copyq_add_plugin(itemweb)
endif(HAS_WEBKIT)

