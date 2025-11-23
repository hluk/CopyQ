file(GLOB copyq_SOURCES ${copyq_SOURCES}
    platform/mac/*.mm
    platform/dummy/dummyclipboard.cpp
    platform/platformcommon.cpp
    ../qxt/qxtglobalshortcut_mac.cpp
    )

set(USE_QXT TRUE)

if (NOT WITH_QT6)
    list(APPEND copyq_qt_modules MacExtras)
endif()

# Install QCA ossl plugin into the bundle
if (WITH_QCA_ENCRYPTION)
    # Find QCA plugin directory
    get_target_property(QCA_LOCATION qca-qt${QT_DEFAULT_MAJOR_VERSION} LOCATION)
    get_filename_component(QCA_LIB_DIR "${QCA_LOCATION}" DIRECTORY)

    # QCA plugins are typically in ../qt/plugins/crypto relative to the library
    # On Homebrew: /usr/local/lib/qt/plugins or /opt/homebrew/lib/qt/plugins
    set(QCA_PLUGIN_SEARCH_PATHS
        "${QCA_LIB_DIR}/qt/plugins/crypto"
        "${QCA_LIB_DIR}/../qt/plugins/crypto"
        "/usr/local/lib/qt/plugins/crypto"
        "/opt/homebrew/lib/qt/plugins/crypto"
    )

    set(QCA_OSSL_PLUGIN_FOUND FALSE)
    foreach(QCA_PLUGIN_PATH ${QCA_PLUGIN_SEARCH_PATHS})
        if(EXISTS "${QCA_PLUGIN_PATH}/libqca-ossl.dylib")
            install(FILES "${QCA_PLUGIN_PATH}/libqca-ossl.dylib"
                DESTINATION "${COPYQ_EXECUTABLE_NAME}.app/Contents/PlugIns/crypto"
                COMPONENT Runtime)
            message(STATUS "QCA ossl plugin found at: ${QCA_PLUGIN_PATH}/libqca-ossl.dylib")
            set(QCA_OSSL_PLUGIN_FOUND TRUE)
            break()
        endif()
    endforeach()

    if(NOT QCA_OSSL_PLUGIN_FOUND)
        message(FATAL_ERROR "QCA ossl plugin (libqca-ossl.dylib) not found. Searched paths: ${QCA_PLUGIN_SEARCH_PATHS}")
    endif()
endif()
