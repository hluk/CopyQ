cmake_minimum_required(VERSION 3.13)
project(copyq)

if (APPLE)
    set(COPYQ_EXECUTABLE_NAME "CopyQ")
else()
    set(COPYQ_EXECUTABLE_NAME copyq)
endif()

# C++17
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(COPYQ_DEBUG ON)
    add_definitions( -DCOPYQ_DEBUG  )
endif()

OPTION(PEDANTIC "Enable all compiler warnings" OFF)

# Options (cmake -LH)
OPTION(WITH_TESTS "Run test cases from command line" ${COPYQ_DEBUG})
OPTION(WITH_PLUGINS "Compile plugins" ON)

add_definitions( -DQT_USE_STRINGBUILDER  )

# Unix-specific options
if (UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    set(DATA_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}" CACHE PATH "Install path for data")
    set(PLUGIN_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${CMAKE_SHARED_MODULE_PREFIX}/copyq/plugins" CACHE PATH "Install path for plugins")
    set(ICON_INSTALL_PREFIX "${DATA_INSTALL_PREFIX}/icons/hicolor/scalable/apps" CACHE PATH "Install path for icons")
    set(ICON_INSTALL_PREFIX_TEMPLATE "${DATA_INSTALL_PREFIX}/icons/hicolor/%SIZE%/apps" CACHE PATH "Install path for icons (%SIZE% is icon size)")
    set(THEME_INSTALL_PREFIX "${DATA_INSTALL_PREFIX}/copyq/themes" CACHE PATH "Install path for themes")
    set(DESKTOP_INSTALL_PREFIX "${DATA_INSTALL_PREFIX}/applications" CACHE PATH "Install path for desktop file")
    set(APPDATA_INSTALL_PREFIX "${DATA_INSTALL_PREFIX}/metainfo" CACHE PATH "Install path for AppData file")
    set(MANPAGE_INSTALL_PREFIX "${CMAKE_INSTALL_MANDIR}/man1" CACHE PATH "Install path for manual pages")
    set(TRANSLATION_INSTALL_PREFIX "${DATA_INSTALL_PREFIX}/copyq/translations" CACHE PATH "Install path for translations")
    set(BASH_COMPLETION_INSTALL_PREFIX "${DATA_INSTALL_PREFIX}/bash-completion/completions/" CACHE PATH "Install path for bash completions")
    set(ICON_NAME "copyq" CACHE STRING "Name for icon files")
    set(COPYQ_AUTOSTART_COMMAND "" CACHE STRING "Autostart command")
    OPTION(COPYQ_AUTOSTART "Enable autostart option" ON)
endif()

set(CMAKE_AUTOMOC ON)

OPTION(WITH_QT6 "Enable Qt 6" OFF)
if (WITH_QT6)
    set(QT_DEFAULT_MAJOR_VERSION 6)
    find_package(Qt6Widgets 6.1.0 REQUIRED)
    set(copyq_qt Qt6)
    add_definitions( -DQT_DEPRECATED_WARNINGS_SINCE=0x051500 )
else()
    set(QT_DEFAULT_MAJOR_VERSION 5)
    find_package(Qt5Widgets 5.8.0 REQUIRED)
    set(copyq_qt Qt5)
    if (Qt5Widgets_VERSION VERSION_LESS 5.15.0)
        function(qt_wrap_ui outfiles)
            qt5_wrap_ui("${outfiles}" ${ARGN})
            set("${outfiles}" "${${outfiles}}" PARENT_SCOPE)
        endfunction()
        function(qt_add_resources outfiles)
            qt5_add_resources("${outfiles}" ${ARGN})
            set("${outfiles}" "${${outfiles}}" PARENT_SCOPE)
        endfunction()
        function(qt_add_translation _qm_files)
            qt5_add_translation("${_qm_files}" ${ARGN})
            set("${_qm_files}" "${${_qm_files}}" PARENT_SCOPE)
        endfunction()
    endif()
endif()

list(APPEND copyq_LIBRARIES ${copyq_qt}::Widgets)

set(copyq_APP_ID          com.github.hluk.copyq)
set(copyq_ICON_PREFIX     src/images/icon)
set(copyq_ICON_NORMAL     src/images/icon.svg)
set(copyq_ICON_MASK       src/images/icon_mask.svg)
set(copyq_DESKTOP         shared/${copyq_APP_ID}.desktop)
set(copyq_APPDATA         shared/${copyq_APP_ID}.appdata.xml)
set(copyq_BASH_COMPLETION shared/copyq-completion)
set(copyq_MANPAGE         debian/copyq.1)

# Be more strict while compiling debugging version
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-long-long")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -Wextra -Wall -pedantic -Wfloat-equal -Woverloaded-virtual -Wundef -Wno-inconsistent-missing-destructor-override")
endif()

if (PEDANTIC)
    if (CMAKE_COMPILER_IS_GNUCXX)
        list(APPEND copyq_pedantic_flags
            -Wextra -Wall
            -Wsuggest-override
            -Wlogical-op
            -Wnoexcept
            -Wstrict-null-sentinel
            -Wno-unknown-pragmas
            )
    else()
        list(APPEND copyq_pedantic_flags
            -Weverything
            -Winconsistent-missing-override
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            -Wno-disabled-macro-expansion
            -Wno-double-promotion
            -Wno-exit-time-destructors
            -Wno-extra-semi-stmt
            -Wno-inconsistent-missing-destructor-override
            -Wno-redundant-parens
            -Wno-shadow-field
            -Wno-shadow-field-in-constructor
            -Wno-shorten-64-to-32
            -Wno-sign-conversion
            -Wno-suggest-destructor-override
            -Wno-unknown-warning-option
            -Wno-used-but-marked-unused
            -Wno-weak-vtables
            -fcomment-block-commands=retval
            )
    endif()

    list(APPEND copyq_pedantic_flags
        -pedantic
        -Wcast-align
        -Wctor-dtor-privacy
        -Wdisabled-optimization
        -Wformat=2
        -Winit-self
        -Wmissing-declarations
        -Wmissing-include-dirs
        -Wold-style-cast
        -Woverloaded-virtual
        -Wredundant-decls
        -Wstrict-overflow=4
        -Wundef
        )

    list(APPEND copyq_pedantic_flags
        -Wno-padded
        -Wno-switch-enum
        )

    # Disable Q_OBJECT macro warnings.
    list(APPEND copyq_pedantic_flags
        -Wno-unused-member-function
        )
endif()

if(WITH_TESTS)
    message(STATUS "Building with tests.")

    add_definitions( -DHAS_TESTS )

    find_package(${copyq_qt}Test REQUIRED)
    list(APPEND copyq_LIBRARIES ${copyq_qt}::Test)
endif()

include(src/version.cmake)
message(STATUS "Building CopyQ version ${copyq_version}.")

if (UNIX AND NOT APPLE)
    install(FILES ${copyq_ICON_NORMAL}     DESTINATION ${ICON_INSTALL_PREFIX} RENAME ${ICON_NAME}.svg)
    install(FILES ${copyq_ICON_MASK}       DESTINATION ${ICON_INSTALL_PREFIX} RENAME ${ICON_NAME}_mask.svg)
    install(FILES ${copyq_APPDATA}         DESTINATION ${APPDATA_INSTALL_PREFIX})
    install(FILES ${copyq_MANPAGE}         DESTINATION ${MANPAGE_INSTALL_PREFIX})
    install(FILES ${copyq_BASH_COMPLETION} DESTINATION ${BASH_COMPLETION_INSTALL_PREFIX} RENAME copyq)

    configure_file(${copyq_DESKTOP}.in ${copyq_DESKTOP})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${copyq_DESKTOP} DESTINATION ${DESKTOP_INSTALL_PREFIX})

    foreach (copyq_ICON_EXTENT 16 22 24 32 48 64 128)
        set(copyq_ICON_SIZE "${copyq_ICON_EXTENT}x${copyq_ICON_EXTENT}")
        string(REPLACE "%SIZE%" "${copyq_ICON_SIZE}" copyq_ICON_TARGET_PREFIX "${ICON_INSTALL_PREFIX_TEMPLATE}")
        foreach (copyq_ICON_TYPE "")
            install(FILES "${copyq_ICON_PREFIX}${copyq_ICON_TYPE}_${copyq_ICON_SIZE}.png" DESTINATION "${copyq_ICON_TARGET_PREFIX}" RENAME "${ICON_NAME}${copyq_ICON_TYPE}.png")
        endforeach()
    endforeach()

    set(copyq_THEME_INSTALL_PREFIX ${THEME_INSTALL_PREFIX})
    file(GLOB copyq_THEMES shared/themes/*.css shared/themes/*.ini)
    install(FILES ${copyq_THEMES} DESTINATION ${THEME_INSTALL_PREFIX})

    add_definitions( -DCOPYQ_ICON_PREFIX="${ICON_INSTALL_PREFIX}/${ICON_NAME}" )
    add_definitions( -DCOPYQ_THEME_PREFIX="${THEME_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_PLUGIN_PREFIX="${PLUGIN_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_DESKTOP_FILE="${DESKTOP_INSTALL_PREFIX}/${copyq_APP_ID}.desktop" )
    add_definitions( -DCOPYQ_TRANSLATION_PREFIX="${TRANSLATION_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_BASH_COMPLETION_PREFIX="${BASH_COMPLETION_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_ICON_NAME="${ICON_NAME}" )

    if (COPYQ_AUTOSTART)
        add_definitions( -DCOPYQ_AUTOSTART )
    endif()

    if (COPYQ_AUTOSTART_COMMAND)
        add_definitions( -DCOPYQ_AUTOSTART_COMMAND="${COPYQ_AUTOSTART_COMMAND}" )
    endif()
elseif (APPLE)
    set(copyq_themes_dest_dir "${COPYQ_EXECUTABLE_NAME}.app/Contents/Resources/themes")
    file(GLOB copyq_THEMES shared/themes/*.css shared/themes/*.ini)
    install(FILES ${copyq_THEMES} DESTINATION "${copyq_themes_dest_dir}" COMPONENT Runtime)
endif()

if (MSVC)
    include(InstallRequiredSystemLibraries)
endif()

add_definitions( -DQT_NO_CAST_TO_ASCII )

set_property(GLOBAL PROPERTY COPYQ_INSTALLED_PLUGINS)

if (WITH_PLUGINS)
    add_subdirectory(plugins)
endif()

add_subdirectory(src)
