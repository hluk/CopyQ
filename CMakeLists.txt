cmake_minimum_required(VERSION 2.8)
PROJECT(copyq)
FIND_PACKAGE(Qt4 REQUIRED)

SET(copyq_SOURCES
    src/aboutdialog.cpp
    src/action.cpp
    src/actiondialog.cpp
    src/app.cpp
    src/arguments.cpp
    src/client_server.cpp
    src/clipboardbrowser.cpp
    src/clipboardclient.cpp
    src/clipboarddialog.cpp
    src/clipboarditem.cpp
    src/clipboardmodel.cpp
    src/clipboardmonitor.cpp
    src/clipboardserver.cpp
    src/configurationmanager.cpp
    src/tabdialog.cpp
    src/itemdelegate.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/qeditor.cpp
    src/shortcutdialog.cpp
    qxt/qxtglobal.cpp
    qxt/qxtglobalshortcut.cpp
    )

# headers that needs to be processed by moc
SET(copyq_HEADERS
    src/aboutdialog.h
    src/actiondialog.h
    src/action.h
    src/app.h
    src/clipboardbrowser.h
    src/clipboardclient.h
    src/clipboarddialog.h
    src/clipboardmodel.h
    src/clipboardmonitor.h
    src/clipboardserver.h
    src/configurationmanager.h
    src/tabdialog.h
    src/itemdelegate.h
    src/mainwindow.h
    src/qeditor.h
    src/shortcutdialog.h
    qxt/qxtglobalshortcut.h
    )

SET(copyq_FORMS
    src/aboutdialog.ui
    src/actiondialog.ui
    src/clipboarddialog.ui
    src/configurationmanager.ui
    src/mainwindow.ui
    src/shortcutdialog.ui
    src/tabdialog.ui
    )

SET(copyq_RESOURCES src/copyq.qrc)

# Qt modules
SET(QT_USE_QTNETWORK TRUE)

INCLUDE(${QT_USE_FILE})
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR} src)
ADD_DEFINITIONS(${QT_DEFINITIONS})

QT4_WRAP_CPP(copyq_HEADERS_MOC ${copyq_HEADERS})
QT4_WRAP_UI(copyq_FORMS_HEADERS ${copyq_FORMS})
QT4_ADD_RESOURCES(copyq_RESOURCES_RCC ${copyq_RESOURCES})

IF(Q_WS_X11)
    # window system is X11 and 'fixes' extension is required
    FIND_PACKAGE(X11)
    if(NOT X11_FOUND)
        message(FATAL_ERROR "X11 is required")
    endif(NOT X11_FOUND)

    if(NOT X11_Xfixes_FOUND)
        message(FATAL_ERROR "X11 'fixes' extension library is required")
    endif(NOT X11_Xfixes_FOUND)

    SET(copyq_LIBRARIES ${copyq_LIBRARIES} ${X11_LIBRARIES} ${X11_Xfixes_LIB})
    LIST(APPEND copyq_SOURCES qxt/qxtglobalshortcut_x11.cpp)
ELSEIF(Q_WS_WIN)
    LIST(APPEND copyq_SOURCES qxt/qxtglobalshortcut_win.cpp)
ELSEIF(Q_WS_MAC)
    LIST(APPEND copyq_SOURCES qxt/qxtglobalshortcut_mac.cpp)
ENDIF(Q_WS_X11)

ADD_EXECUTABLE(copyq ${copyq_SOURCES}
    ${copyq_HEADERS_MOC}
    ${copyq_FORMS_HEADERS}
    ${copyq_RESOURCES_RCC})
TARGET_LINK_LIBRARIES(copyq ${QT_LIBRARIES} ${copyq_LIBRARIES})

install(TARGETS copyq DESTINATION bin)

