cmake_minimum_required(VERSION 2.8)
project(copyq)
find_package(Qt4 REQUIRED QtCore QtGui QtNetwork QtSvg QtXml QtScript)

file(GLOB copyq_SOURCES
    src/*.cpp
    qt/*.cpp
    qxt/qxtglobal.cpp
    qxt/qxtglobalshortcut.cpp
    )

set(copyq_MOCABLE
    src/include/aboutdialog.h
    src/include/actiondialog.h
    src/include/action.h
    src/include/app.h
    src/include/clipboardbrowser.h
    src/include/clipboardclient.h
    src/include/clipboarddialog.h
    src/include/clipboardmodel.h
    src/include/clipboardmonitor.h
    src/include/clipboardserver.h
    src/include/commandwidget.h
    src/include/configurationmanager.h
    src/include/tabbar.h
    src/include/tabdialog.h
    src/include/tabwidget.h
    src/include/itemdelegate.h
    src/include/mainwindow.h
    src/include/itemeditor.h
    src/include/scriptable.h
    src/include/shortcutdialog.h
    qxt/qxtglobalshortcut.h
    qt/bytearrayprototype.h
    qt/bytearrayclass.h
    )

file(GLOB copyq_FORMS
    src/ui/*.ui
    )

set(copyq_RESOURCES src/copyq.qrc)

file(GLOB copyq_TRANSLATIONS translations/*.ts)

# Qt modules
set(QT_USE_QTNETWORK TRUE)
set(QT_USE_QTSVG TRUE)
set(QT_USE_QTXML TRUE)
set(QT_USE_QTSCRIPT TRUE)

include(${QT_USE_FILE})
include_directories(${QT_INCLUDES} ${CMAKE_BINARY_DIR} src/include)
add_definitions(${QT_DEFINITIONS})

qt4_wrap_cpp(copyq_MOC ${copyq_MOCABLE})
qt4_wrap_ui(copyq_FORMS_HEADERS ${copyq_FORMS})
qt4_add_resources(copyq_RESOURCES_RCC ${copyq_RESOURCES})
qt4_add_translation(copyq_QM ${copyq_TRANSLATIONS})

# Be more strict while compiling debugging version
if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra -Wall -pedantic")
endif()

# Generate a qrc file for the translations
set(copyq_QM_RCC ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc)
file(WRITE ${copyq_QM_RCC} "<RCC><qresource prefix=\"/translations\">")
foreach(QM_FILE ${copyq_QM})
  file(RELATIVE_PATH copyq_RELATIVE_PATH ${CMAKE_CURRENT_BINARY_DIR} ${QM_FILE})
  file(APPEND ${copyq_QM_RCC} "<file>" ${copyq_RELATIVE_PATH} "</file>")
endforeach(QM_FILE)
file(APPEND ${copyq_QM_RCC} "</qresource></RCC>")
qt4_add_resources(copyq_RESOURCES_RCC ${copyq_QM_RCC})

if(Q_WS_X11)
    # window system is X11 and 'fixes' extension is required
    find_package(X11)
    if(NOT X11_FOUND)
        message(FATAL_ERROR "X11 is required")
    endif(NOT X11_FOUND)

    if(NOT X11_Xfixes_FOUND)
        message(FATAL_ERROR "X11 'fixes' extension library is required")
    endif(NOT X11_Xfixes_FOUND)

    set(copyq_LIBRARIES ${copyq_LIBRARIES} ${X11_LIBRARIES} ${X11_Xfixes_LIB})
    list(APPEND copyq_SOURCES qxt/qxtglobalshortcut_x11.cpp)
elseif(Q_WS_WIN)
    list(APPEND copyq_SOURCES qxt/qxtglobalshortcut_win.cpp)
elseif(Q_WS_MAC)
    list(APPEND copyq_SOURCES qxt/qxtglobalshortcut_mac.cpp)
endif(Q_WS_X11)

add_executable(copyq ${copyq_SOURCES}
    ${copyq_MOC}
    ${copyq_FORMS_HEADERS}
    ${copyq_RESOURCES_RCC}
    ${copyq_QM}
    )
target_link_libraries(copyq ${QT_LIBRARIES} ${copyq_LIBRARIES})

install(TARGETS copyq DESTINATION bin)

