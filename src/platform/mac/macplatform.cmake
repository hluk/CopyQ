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
    get_target_property(QCA_LOCATION qca-qt${QT_DEFAULT_MAJOR_VERSION} IMPORTED_LOCATION)
    if(NOT QCA_LOCATION)
        get_target_property(QCA_LOCATION qca-qt${QT_DEFAULT_MAJOR_VERSION} IMPORTED_LOCATION_RELEASE)
    endif()
    if(NOT QCA_LOCATION)
        get_target_property(QCA_LOCATION qca-qt${QT_DEFAULT_MAJOR_VERSION} IMPORTED_LOCATION_NOCONFIG)
    endif()
    get_filename_component(QCA_LIB_DIR "${QCA_LOCATION}" DIRECTORY)

    # Derive the install prefix from the QCA cmake config location.
    # Qca-qt6_DIR is set by find_package and points to <prefix>/lib/cmake/Qca-qt6.
    get_filename_component(QCA_INSTALL_PREFIX "${Qca-qt6_DIR}/../../.." ABSOLUTE)

    # QCA plugins are in one of these locations:
    # - <prefix>/lib/qca-qt6/crypto (source builds)
    # - <lib_dir>/qt/plugins/crypto (Homebrew framework)
    set(QCA_PLUGIN_SEARCH_PATHS
        "${QCA_INSTALL_PREFIX}/lib/qca-qt6/crypto"
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

# Bundle non-Qt third-party libraries into the app.
# macdeployqt only handles Qt libraries; we must handle the rest.
if (WITH_QT6)
    set(_bundle_fwk_dir "${COPYQ_EXECUTABLE_NAME}.app/Contents/Frameworks")

    # Helper: bundle a target's library into the Frameworks directory.
    function(copyq_bundle_library target)
        if(NOT TARGET ${target})
            return()
        endif()
        get_target_property(_loc ${target} IMPORTED_LOCATION)
        if(NOT _loc)
            get_target_property(_loc ${target} IMPORTED_LOCATION_RELEASE)
        endif()
        if(NOT _loc)
            get_target_property(_loc ${target} IMPORTED_LOCATION_NOCONFIG)
        endif()
        if(NOT _loc)
            return()
        endif()
        if(_loc MATCHES "\\.framework/")
            string(REGEX REPLACE "(.*\\.framework)/.*" "\\1" _fwk_dir "${_loc}")
            install(DIRECTORY "${_fwk_dir}"
                DESTINATION "${_bundle_fwk_dir}"
                COMPONENT Runtime)
        else()
            # Install the actual library and all versioned symlinks.
            get_filename_component(_dir "${_loc}" DIRECTORY)
            get_filename_component(_name "${_loc}" NAME)
            # Get the base name (e.g. libqt6keychain from libqt6keychain.1.0.0.dylib)
            string(REGEX REPLACE "\\.[0-9].*" "" _base "${_name}")
            file(GLOB _all_files "${_dir}/${_base}*")
            install(FILES ${_all_files}
                DESTINATION "${_bundle_fwk_dir}"
                COMPONENT Runtime)
        endif()
    endfunction()

    copyq_bundle_library(qca-qt6)
    copyq_bundle_library(qt6keychain)
    copyq_bundle_library(KF6::ConfigCore)
    copyq_bundle_library(KF6::WindowSystem)
    copyq_bundle_library(KF6::Notifications)
    copyq_bundle_library(KF6::StatusNotifierItem)
endif()
