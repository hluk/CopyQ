project(copyq)

OPTION(WITH_WEBKIT "WebKit support" ON)
OPTION(WITH_QT5 "Qt5 support" OFF)
OPTION(WITH_TESTS "Run test cases from command line" OFF)

if (WITH_QT5)
    cmake_minimum_required(VERSION 2.8.8)
else()
    cmake_minimum_required(VERSION 2.8)
endif()

# Qt modules
if (WITH_QT5)
    find_package(Qt5Widgets REQUIRED)
    set(copyq_Qt5_Modules Widgets Network Svg Xml Script)

    # webkit
    if (WITH_WEBKIT)
        find_package(Qt5WebKitWidgets QUIET)
        set(HAS_WEBKIT ${Qt5WebKitWidgets_FOUND})
    endif(WITH_WEBKIT)

    set(USE_QXT FALSE)
else()
    find_package(Qt4 REQUIRED)
    set(QT_USE_QTNETWORK TRUE)
    set(QT_USE_QTSVG TRUE)
    set(QT_USE_QTXML TRUE)
    set(QT_USE_QTSCRIPT TRUE)

    # webkit
    if (WITH_WEBKIT)
        set(HAS_WEBKIT ${QT_QTWEBKIT_FOUND})
    endif(WITH_WEBKIT)
    set(QT_USE_QTWEBKIT ${HAS_WEBKIT})

    set(USE_QXT TRUE)
endif()

# Project files
file(GLOB copyq_SOURCES
    src/*.cpp
    qt/*.cpp
    )

set(copyq_MOCABLE
    qt/bytearrayclass.h
    qt/bytearrayprototype.h
    src/include/aboutdialog.h
    src/include/actiondialog.h
    src/include/action.h
    src/include/clipboardbrowser.h
    src/include/clipboardclient.h
    src/include/clipboarddialog.h
    src/include/clipboardmodel.h
    src/include/clipboardmonitor.h
    src/include/clipboardserver.h
    src/include/commandwidget.h
    src/include/configurationmanager.h
    src/include/itemimage.h
    src/include/itemtext.h
    src/include/itemdelegate.h
    src/include/itemeditor.h
    src/include/mainwindow.h
    src/include/remoteprocess.h
    src/include/scriptable.h
    src/include/shortcutdialog.h
    src/include/tabbar.h
    src/include/tabdialog.h
    src/include/tabwidget.h
    )

file(GLOB copyq_FORMS
    src/ui/*.ui
    )

set(copyq_RESOURCES src/copyq.qrc)

set(copyq_ICON src/images/logo.svg)
set(copyq_DESKTOP shared/copyq.desktop)

file(GLOB copyq_TRANSLATIONS translations/*.ts)

# webkit
if (HAS_WEBKIT)
    message(STATUS "Building with WebKit.")
    add_definitions(-DHAS_WEBKIT)

    set(copyq_MOCABLE ${copyq_MOCABLE}
        src/include/itemweb.h
        )

    if (WITH_QT5)
        set(copyq_Qt5_Modules ${copyq_Qt5_Modules} WebKitWidgets)
    endif()
else()
    message(STATUS "Building without WebKit.")
endif(HAS_WEBKIT)

# Qt include paths and definitions
if (WITH_QT5)
    include_directories(${Qt5Widgets_INCLUDES} ${CMAKE_BINARY_DIR} src/include)
    add_definitions(${Qt5Widgets_DEFINITIONS})
else()
    include_directories(${QT_INCLUDES} ${CMAKE_BINARY_DIR} src/include)
    add_definitions(${QT_DEFINITIONS})
endif()

# If window system is X11 then set X11_FOUND to TRUE.
find_package(X11)

if (X11_FOUND)
    # X11 requires 'fixes' extension for clipboard to work correctly.
    message(STATUS "Building for X11 window system.")
    if(NOT X11_Xfixes_FOUND)
        message(FATAL_ERROR "X11 'fixes' extension library is required")
    endif(NOT X11_Xfixes_FOUND)
elseif (Q_WS_WIN)
    # File icon on Windows
    set(copyq_RC src/copyq.rc)
endif(X11_FOUND)

# Qxt - global shortcuts
if (USE_QXT)
    set(copyq_SOURCES ${copyq_SOURCES}
        qxt/qxtglobal.cpp
        qxt/qxtglobalshortcut.cpp
        )
    set(copyq_MOCABLE ${copyq_MOCABLE}
        qxt/qxtglobalshortcut.h
        )
    add_definitions( -DQXT_STATIC )

    if (X11_FOUND)
        list(APPEND copyq_SOURCES qxt/qxtglobalshortcut_x11.cpp)
    elseif (Q_WS_WIN)
        list(APPEND copyq_SOURCES qxt/qxtglobalshortcut_win.cpp)
    elseif (Q_WS_MAC)
        list(APPEND copyq_SOURCES qxt/qxtglobalshortcut_mac.cpp)
    endif()
endif(USE_QXT)

# Be more strict while compiling debugging version
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra -Wall -pedantic")
endif()

# Tests always on with debug version
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(WITH_TESTS ON)
endif()

# Compile with tests?
if (WITH_TESTS)
    message(STATUS "Building with tests.")
    add_definitions( -DHAS_TESTS )

    file(GLOB copyq_SOURCES ${copyq_SOURCES}
        src/tests/*.cpp
        )
    file(GLOB copyq_MOCABLE ${copyq_MOCABLE}
        src/tests/*.h
        )

    if (WITH_QT5)
        set(copyq_Qt5_Modules ${copyq_Qt5_Modules} Test)
    else()
        set(QT_USE_QTTEST TRUE)
    endif()
endif()

set(copyq_LIBRARIES ${copyq_LIBRARIES} ${X11_LIBRARIES} ${X11_Xfixes_LIB})

if (WITH_QT5)
    qt5_wrap_cpp(copyq_MOC ${copyq_MOCABLE})
    qt5_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
    qt5_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES})
    find_package(Qt5LinguistTools)
    if (Qt5LinguistTools_FOUND)
        qt5_add_translation(copyq_QM ${copyq_TRANSLATIONS})
    endif()
else()
    qt4_wrap_cpp(copyq_MOC ${copyq_MOCABLE})
    qt4_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
    qt4_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES})
    qt4_add_translation(copyq_QM ${copyq_TRANSLATIONS})
endif()

# Generate a qrc file for the translations
set(copyq_QM_RCC ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc)
file(WRITE ${copyq_QM_RCC} "<RCC><qresource prefix=\"/translations\">")
foreach(QM_FILE ${copyq_QM})
    file(RELATIVE_PATH copyq_RELATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR} ${QM_FILE})
    file(APPEND ${copyq_QM_RCC} "<file>" ${copyq_RELATIVE_PATH} "</file>")
endforeach(QM_FILE)
file(APPEND ${copyq_QM_RCC} "</qresource></RCC>")
if (WITH_QT5)
    qt5_add_resources(copyq_RESOURCES_RCC ${copyq_QM_RCC})
else()
    qt4_add_resources(copyq_RESOURCES_RCC ${copyq_QM_RCC})
endif()

add_executable(copyq ${copyq_SOURCES}
    ${copyq_MOC}
    ${copyq_FORMS_HEADERS}
    ${copyq_RESOURCES_RCC}
    ${copyq_RC}
    ${copyq_QM}
    )

if (WITH_QT5)
    qt5_use_modules(copyq ${copyq_Qt5_Modules})
else()
    include(${QT_USE_FILE})
endif()

target_link_libraries(copyq ${QT_LIBRARIES} ${copyq_LIBRARIES})

install(TARGETS copyq DESTINATION bin)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install(FILES ${copyq_ICON} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons" RENAME copyq.svg)
    install(FILES ${copyq_DESKTOP} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

