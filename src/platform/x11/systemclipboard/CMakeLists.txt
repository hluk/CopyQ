include_directories(SYSTEM ${${copyq_qt}Gui_PRIVATE_INCLUDE_DIRS}) # for native interface to get wl_seat
find_package(Wayland 1.15 COMPONENTS Client)
find_package(${copyq_qt} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS WaylandClient)

set(systemclipboard_SRCS
    waylandclipboard.cpp
)

if(WITH_QT6)
    add_library(systemclipboard STATIC ${systemclipboard_SRCS})
    qt_generate_wayland_protocol_client_sources(systemclipboard
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/wlr-data-control-unstable-v1.xml"
            "${CMAKE_CURRENT_SOURCE_DIR}/keyboard-shortcuts-inhibit-unstable-v1.xml"
            "${Wayland_DATADIR}/wayland.xml"
    )
else()
    find_package(QtWaylandScanner REQUIRED)
    ecm_add_qtwayland_client_protocol(systemclipboard_SRCS
        PROTOCOL wlr-data-control-unstable-v1.xml
        BASENAME wlr-data-control-unstable-v1
    )
    ecm_add_qtwayland_client_protocol(systemclipboard_SRCS
        PROTOCOL keyboard-shortcuts-inhibit-unstable-v1.xml
        BASENAME keyboard-shortcuts-inhibit-unstable-v1
    )
    ecm_add_qtwayland_client_protocol(systemclipboard_SRCS
        PROTOCOL "${Wayland_DATADIR}/wayland.xml"
        BASENAME wayland
    )
    add_library(systemclipboard STATIC ${systemclipboard_SRCS})
endif()

target_link_libraries(systemclipboard
                      ${copyq_qt}::Gui
                      ${copyq_qt}::WaylandClient
                      Wayland::Client
)
if (TARGET ${copyq_qt}::GuiPrivate)
    # for native interface to get wl_seat
    target_link_libraries(systemclipboard ${copyq_qt}::GuiPrivate)
endif()
