cmake_minimum_required(VERSION 2.8.6)
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
OPTION(WITH_QT5 "Use Qt 5 (disable to use Qt 4 instead)" ON)
OPTION(WITH_TESTS "Run test cases from command line" ${COPYQ_DEBUG})
OPTION(WITH_PLUGINS "Compile plugins" ON)
# Linux-specific options
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(PLUGIN_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/${CMAKE_SHARED_MODULE_PREFIX}/copyq/plugins" CACHE PATH "Install path for plugins")
    set(ICON_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps" CACHE PATH "Install path for icons")
    set(ICON_INSTALL_PREFIX_TEMPLATE "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/%SIZE%/apps" CACHE PATH "Install path for icons (%SIZE% is icon size)")
    set(THEME_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/copyq/themes" CACHE PATH "Install path for themes")
    set(DESKTOP_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/applications" CACHE PATH "Install path for \"copyq.desktop\"")
    set(APPDATA_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/appdata" CACHE PATH "Install path for \"copyq.appdata.xml\"")
    set(TRANSLATION_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/share/copyq/translations" CACHE PATH "Install path for translations")
endif()

set(CMAKE_AUTOMOC ON)

if (WITH_QT5)
    cmake_minimum_required(VERSION 2.8.8)
    find_package(Qt5Widgets)
    if (NOT Qt5Widgets_FOUND)
        message(FATAL_ERROR "Qt 5 is unavailable. To compile with Qt 4 use -DWITH_QT5=OFF.")
    endif()
    message(STATUS "Building with Qt 5.")
else()
    find_package(Qt4)
    if (NOT QT4_FOUND)
        # Try different executable name.
        set(QT_QMAKE_EXECUTABLE "qmake-qt4")
        find_package(Qt4)
        if (NOT QT4_FOUND)
            message(FATAL_ERROR "Qt 4 is unavailable. To compile with Qt 5 use -DWITH_QT5=ON.")
        endif()
    endif()
    message(STATUS "Building with Qt 4.")
endif()

set(copyq_ICON_PREFIX src/images/icon)
set(copyq_ICON_NORMAL src/images/icon.svg)
set(copyq_ICON_BUSY   src/images/icon-running.svg)
set(copyq_DESKTOP     shared/copyq.desktop)
set(copyq_APPDATA     shared/copyq.appdata.xml)

# Be more strict while compiling debugging version
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-long-long")
    set(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} -Wextra -Wall -pedantic -Wfloat-equal -Woverloaded-virtual -Wundef")
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

    if (WITH_QT5)
        list(APPEND copyq_Qt5_Modules Test)
    else()
        set(QT_USE_QTTEST TRUE)
    endif()
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

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(FILES ${copyq_ICON_NORMAL} DESTINATION ${ICON_INSTALL_PREFIX} RENAME copyq-normal.svg)
    install(FILES ${copyq_ICON_BUSY}   DESTINATION ${ICON_INSTALL_PREFIX} RENAME copyq-busy.svg)
    install(FILES ${copyq_DESKTOP}     DESTINATION ${DESKTOP_INSTALL_PREFIX})
    install(FILES ${copyq_APPDATA}     DESTINATION ${APPDATA_INSTALL_PREFIX})

    foreach (copyq_ICON_EXTENT 16 22 24 32 48 64 128)
        set(copyq_ICON_SIZE "${copyq_ICON_EXTENT}x${copyq_ICON_EXTENT}")
        string(REPLACE "%SIZE%" "${copyq_ICON_SIZE}" copyq_ICON_TARGET_PREFIX "${ICON_INSTALL_PREFIX_TEMPLATE}")
        foreach (copyq_ICON_TYPE "" "-normal" "-busy")
            install(FILES "${copyq_ICON_PREFIX}${copyq_ICON_TYPE}_${copyq_ICON_SIZE}.png" DESTINATION "${copyq_ICON_TARGET_PREFIX}" RENAME "copyq${copyq_ICON_TYPE}.png")
        endforeach()
    endforeach()

    set(copyq_THEME_INSTALL_PREFIX ${THEME_INSTALL_PREFIX})
    file(GLOB copyq_THEMES shared/themes/*.ini)
    install(FILES ${copyq_THEMES} DESTINATION ${THEME_INSTALL_PREFIX})

    add_definitions( -DCOPYQ_ICON_PREFIX="${ICON_INSTALL_PREFIX}/copyq" )
    add_definitions( -DCOPYQ_THEME_PREFIX="${THEME_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_PLUGIN_PREFIX="${PLUGIN_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_DESKTOP_PREFIX="${DESKTOP_INSTALL_PREFIX}" )
    add_definitions( -DCOPYQ_TRANSLATION_PREFIX="${TRANSLATION_INSTALL_PREFIX}" )
endif()

add_definitions( -DQT_NO_CAST_TO_ASCII )

add_subdirectory(src)

if (WITH_PLUGINS)
    add_subdirectory(plugins)
endif()

