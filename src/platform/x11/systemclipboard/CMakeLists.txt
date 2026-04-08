if(Qt6_VERSION VERSION_LESS "6.5.0")
    include_directories(SYSTEM ${${copyq_qt}Gui_PRIVATE_INCLUDE_DIRS}) # for native interface to get wl_seat
endif()
find_package(Wayland 1.15 COMPONENTS Client)
find_package(${copyq_qt} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS WaylandClient)

set(systemclipboard_SRCS
    waylandclipboard.cpp
)

add_library(systemclipboard STATIC ${systemclipboard_SRCS})
qt_generate_wayland_protocol_client_sources(systemclipboard
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/wlr-data-control-unstable-v1.xml"
        "${CMAKE_CURRENT_SOURCE_DIR}/keyboard-shortcuts-inhibit-unstable-v1.xml"
        "${Wayland_DATADIR}/wayland.xml"
)

target_link_libraries(systemclipboard
                      ${copyq_qt}::Gui
                      ${copyq_qt}::WaylandClient
                      Wayland::Client
)
if(Qt6_VERSION VERSION_LESS "6.5.0")
    if (TARGET ${copyq_qt}::GuiPrivate)
        # for native interface to get wl_seat
        target_link_libraries(systemclipboard ${copyq_qt}::GuiPrivate)
    endif()
endif()
