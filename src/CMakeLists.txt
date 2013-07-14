# Project files
file(GLOB copyq_SOURCES
    main.cpp
    app/*.cpp
    common/*.cpp
    gui/*.cpp
    item/*.cpp
    scriptable/*.cpp
    ../qt/*.cpp
    )

set(copyq_MOCABLE
    app/clipboardclient.h
    app/clipboardmonitor.h
    app/clipboardserver.h
    app/remoteprocess.h
    common/action.h
    gui/aboutdialog.h
    gui/actiondialog.h
    gui/clipboardbrowser.h
    gui/clipboarddialog.h
    gui/commandwidget.h
    gui/configtabappearance.h
    gui/configurationmanager.h
    gui/mainwindow.h
    gui/notification.h
    gui/notificationdaemon.h
    gui/pluginwidget.h
    gui/shortcutdialog.h
    gui/tabbar.h
    gui/tabdialog.h
    gui/tabtree.h
    gui/tabwidget.h
    gui/traymenu.h
    item/itemdelegate.h
    item/itemeditor.h
    item/itemfactory.h
    item/clipboardmodel.h
    ../qt/bytearrayclass.h
    ../qt/bytearrayprototype.h
    scriptable/scriptable.h
    scriptable/scriptableworker.h
    )

file(GLOB copyq_FORMS
    ui/*.ui
    )

set(copyq_RESOURCES copyq.qrc)

# Qt include paths and definitions
include_directories(${CMAKE_CURRENT_BINARY_DIR} .)
if (WITH_QT5)
    include_directories(${Qt5Widgets_INCLUDES})
    add_definitions(${Qt5Widgets_DEFINITIONS})
else()
    include_directories(${QT_INCLUDES})
    add_definitions(${QT_DEFINITIONS})
endif()

include(platform/platform.cmake)

# Qxt - global shortcuts
if (USE_QXT)
    set(copyq_SOURCES ${copyq_SOURCES}
        ../qxt/qxtglobal.cpp
        ../qxt/qxtglobalshortcut.cpp
        )
    set(copyq_MOCABLE ${copyq_MOCABLE}
        ../qxt/qxtglobalshortcut.h
        )
    add_definitions( -DQXT_STATIC )
endif()

# Compile with tests?
if (WITH_TESTS)
    message(STATUS "Building with tests.")
    add_definitions( -DHAS_TESTS )

    file(GLOB copyq_SOURCES ${copyq_SOURCES}
        tests/*.cpp
        )
    file(GLOB copyq_MOCABLE ${copyq_MOCABLE}
        tests/*.h
        )

    if (WITH_QT5)
        set(copyq_Qt5_Modules ${copyq_Qt5_Modules} Test)
    else()
        set(QT_USE_QTTEST TRUE)
    endif()
endif()

# translations
file(GLOB copyq_TRANSLATIONS ../translations/*.ts)

if (WITH_QT5)
    find_package(Qt5LinguistTools)
    if (Qt5LinguistTools_FOUND)
        qt5_add_translation(copyq_QM ${copyq_TRANSLATIONS})
    endif()
else()
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
    qt5_wrap_cpp(copyq_MOC ${copyq_MOCABLE})
    qt5_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
    qt5_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES} ${copyq_QM_RCC})
else()
    qt4_wrap_cpp(copyq_MOC ${copyq_MOCABLE})
    qt4_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
    qt4_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES} ${copyq_QM_RCC})
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

set(copyq_COMPILE
    ${copyq_SOURCES}
    ${copyq_MOC}
    ${copyq_FORMS_HEADERS}
    ${copyq_RESOURCES_RCC}
    ${copyq_QM}
    )

if (WIN32)
    # Create separate binaries for GUI and console under Windows.

    # GUI (copyq.exe)
    add_executable(copyq WIN32 ${copyq_COMPILE} ${copyq_RC})

    if (MSVC)
        set_target_properties(copyq PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()

    # console (copyq.com)
    add_executable(copyqcon platform/win/main.cpp)
    set_target_properties(copyqcon PROPERTIES OUTPUT_NAME "copyq" SUFFIX ".com")

    # Qt modules for console app
    if (WITH_QT5)
        qt5_use_modules(copyqcon Core)
    else()
        include(${QT_USE_FILE})
    endif()

    # link console app
    target_link_libraries(copyqcon ${QT_LIBRARIES})

    # install console app
    install(TARGETS copyqcon DESTINATION bin)
else()
    # Single binary for console and GUI.
    add_executable(copyq ${copyq_COMPILE})
endif()

# Qt modules
if (WITH_QT5)
    qt5_use_modules(copyq Widgets Network Svg Xml Script ${copyq_Qt5_Modules})
else()
    set(QT_USE_QTNETWORK TRUE)
    set(QT_USE_QTSVG TRUE)
    set(QT_USE_QTXML TRUE)
    set(QT_USE_QTSCRIPT TRUE)
    include(${QT_USE_FILE})
endif()

# link
target_link_libraries(copyq ${QT_LIBRARIES} ${copyq_LIBRARIES})

# install
install(TARGETS copyq DESTINATION bin)
