# Qt modules
if (WITH_QT5)
    OPTION(WITH_WEBENGINE "Qt WebEngine support" ON)

    if (WITH_WEBENGINE)
        find_package(Qt5WebEngineWidgets QUIET)
        set(HAS_WEBENGINE ${Qt5WebEngineWidgets_FOUND})
        if (HAS_WEBENGINE)
            message(STATUS "Building with ItemWeb2 plugin.")
            set(copyq_plugin_itemweb2_Qt5_Modules WebEngineWidgets)
            copyq_add_plugin(itemweb2)
        endif()
    endif()
endif()

