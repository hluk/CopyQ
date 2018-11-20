cmake_minimum_required(VERSION 2.8.8)
project(copyq)

# C++11
if (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.1)
    set(CMAKE_CXX_STANDARD 11)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(COPYQ_DEBUG ON)
    add_definitions( -DCOPYQ_DEBUG  )
endif()

OPTION(PEDANTIC "Enable all compiler warnings" OFF)

# Options (cmake -LH)
OPTION(WITH_TESTS "Run test cases from command line" ${COPYQ_DEBUG})
OPTION(WITH_PLUGINS "Compile plugins" ON)

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
    set(MANPAGE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man1" CACHE PATH "Install path for manual pages")
    set(TRANSLATION_INSTALL_PREFIX "${DATA_INSTALL_PREFIX}/copyq/translations" CACHE PATH "Install path for translations")
    set(ICON_NAME "copyq" CACHE STRING "Name for icon files")
endif()

set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets 5.1.0 REQUIRED)
list(APPEND copyq_LIBRARIES Qt5::Widgets)

set(copyq_APP_ID      com.github.hluk.copyq)
set(copyq_ICON_PREFIX src/images/icon)
set(copyq_ICON_NORMAL src/images/icon.svg)
set(copyq_ICON_BUSY   src/images/icon-busy.svg)
set(copyq_ICON_MASK   src/images/icon-mask.svg)
set(copyq_ICON_BUSY_MASK src/images/icon-busy-mask.svg)
set(copyq_DESKTOP     shared/${copyq_APP_ID}.desktop)
set(copyq_APPDATA     shared/${copyq_APP_ID}.appdata.xml)
set(copyq_MANPAGE     debian/copyq.1)

# Be more strict while compiling debugging version
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-long-long")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -Wextra -Wall -pedantic -Wfloat-equal -Woverloaded-virtual -Wundef -Wno-inconsistent-missing-destructor-override")
endif()

if (PEDANTIC)
    if (CMAKE_COMPILER_IS_GNUCXX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wall \
            -Wsuggest-override \
            -Wlogical-op \
            -Wnoexcept \
            -Wstrict-null-sentinel \
            ")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything \
            -Winconsistent-missing-override \
            ")

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
            -Wno-c++98-compat \
            -Wno-c++98-compat-pedantic \
            -Wno-shadow-field-in-constructor \
            -Wno-weak-vtables \
            -Wno-disabled-macro-expansion \
            -fcomment-block-commands=retval \
            ")

        # Disable errors from moc-generated files.
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
            -Wno-undefined-reinterpret-cast \
            -Wno-missing-prototypes \
            ")

        # Disable errors from qrc-generated files.
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
            -Wno-exit-time-destructors \
            -Wno-global-constructors \
            ")
    endif()

    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -pedantic -Werror \
        -Wcast-align \
        -Wcast-qual \
        -Wctor-dtor-privacy \
        -Wdisabled-optimization \
        -Wformat=2 \
        -Winit-self \
        -Wmissing-declarations \
        -Wmissing-include-dirs \
        -Wold-style-cast \
        -Woverloaded-virtual \
        -Wredundant-decls \
        -Wstrict-overflow=4 \
        -Wundef \
        ")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -Wno-padded \
        -Wno-switch-enum \
        ")

    # Disable Q_OBJECT macro warnings.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
        -Wno-unused-member-function \
        ")
endif()

if(WITH_TESTS)
    message(STATUS "Building with tests.")

    add_definitions( -DHAS_TESTS )

    find_package(Qt5Test REQUIRED)
    list(APPEND copyq_LIBRARIES Qt5::Test)
endif()

# Get application version.
if (EXISTS "version.txt")
    file(STRINGS "version.txt" copyq_version)
endif()

if (NOT copyq_version)
    find_package(Git)
    if(GIT_FOUND)
        execute_process(COMMAND
            "${GIT_EXECUTABLE}" describe
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE copyq_git_describe_result
            OUTPUT_VARIABLE copyq_git_describe_output
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
            )
        if(copyq_git_describe_result EQUAL 0)
            set(copyq_version "${copyq_git_describe_output}")
        endif()
    endif()
endif()

if (copyq_version)
    message(STATUS "Building CopyQ version ${copyq_version}.")
    add_definitions( -DCOPYQ_VERSION="${copyq_version}" )
endif()

if (UNIX AND NOT APPLE)
    install(FILES ${copyq_ICON_NORMAL} DESTINATION ${ICON_INSTALL_PREFIX} RENAME ${ICON_NAME}.svg)
    install(FILES ${copyq_ICON_BUSY}   DESTINATION ${ICON_INSTALL_PREFIX} RENAME ${ICON_NAME}-busy.svg)
    install(FILES ${copyq_ICON_MASK}   DESTINATION ${ICON_INSTALL_PREFIX} RENAME ${ICON_NAME}-mask.svg)
    install(FILES ${copyq_ICON_BUSY_MASK} DESTINATION ${ICON_INSTALL_PREFIX} RENAME ${ICON_NAME}-busy-mask.svg)
    install(FILES ${copyq_APPDATA}     DESTINATION ${APPDATA_INSTALL_PREFIX})
    install(FILES ${copyq_MANPAGE}     DESTINATION ${MANPAGE_INSTALL_PREFIX})

    configure_file(${copyq_DESKTOP}.in ${copyq_DESKTOP})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${copyq_DESKTOP} DESTINATION ${DESKTOP_INSTALL_PREFIX})

    foreach (copyq_ICON_EXTENT 16 22 24 32 48 64 128)
        set(copyq_ICON_SIZE "${copyq_ICON_EXTENT}x${copyq_ICON_EXTENT}")
        string(REPLACE "%SIZE%" "${copyq_ICON_SIZE}" copyq_ICON_TARGET_PREFIX "${ICON_INSTALL_PREFIX_TEMPLATE}")
        foreach (copyq_ICON_TYPE "" "-busy")
            install(FILES "${copyq_ICON_PREFIX}${copyq_ICON_TYPE}_${copyq_ICON_SIZE}.png" DESTINATION "${copyq_ICON_TARGET_PREFIX}" RENAME "${ICON_NAME}${copyq_ICON_TYPE}.png")
        endforeach()
    endforeach()

    set(copyq_THEME_INSTALL_PREFIX ${THEME_INSTALL_PREFIX})
    file(GLOB copyq_THEMES shared/themes/*.ini)
    install(FILES ${copyq_THEMES} DESTINATION ${THEME_INSTALL_PREFIX})

    add_definitions( -DCOPYQ_ICON_PREFIX="${ICON_INSTALL_PREFIX}/${ICON_NAME}" )
    add_definitions( -DCOPYQ_THEME_PREFIX="${THEME_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_PLUGIN_PREFIX="${PLUGIN_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_DESKTOP_FILE="${DESKTOP_INSTALL_PREFIX}/${copyq_APP_ID}.desktop" )
    add_definitions( -DCOPYQ_TRANSLATION_PREFIX="${TRANSLATION_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_ICON_NAME="${ICON_NAME}" )
endif()

add_definitions( -DQT_NO_CAST_TO_ASCII )

add_subdirectory(src)

if (WITH_PLUGINS)
    add_subdirectory(plugins)
endif()

