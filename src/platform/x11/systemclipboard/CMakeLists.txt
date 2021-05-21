find_package(QtWaylandScanner REQUIRED)
include_directories(SYSTEM ${${copyq_qt}Gui_PRIVATE_INCLUDE_DIRS}) # for native interface to get wl_seat
find_package(Wayland 1.15 COMPONENTS Client)
find_package(${copyq_qt} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS WaylandClient)

set(systemclipboard_SRCS
    waylandclipboard.cpp
)

ecm_add_qtwayland_client_protocol(systemclipboard_SRCS
    PROTOCOL  wlr-data-control-unstable-v1.xml
    BASENAME wlr-data-control-unstable-v1
)

add_library(systemclipboard STATIC ${systemclipboard_SRCS})
target_link_libraries(systemclipboard
                      ${copyq_qt}::Gui
                      ${copyq_qt}::WaylandClient
                      Wayland::Client
)
