find_package(QtWaylandScanner REQUIRED)

find_package(Qt5Gui REQUIRED)
include_directories(SYSTEM ${Qt5Gui_PRIVATE_INCLUDE_DIRS}) # for native interface to get wl_seat

find_package(Wayland 1.15 COMPONENTS Client)
find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS WaylandClient)

set(systemclipboard_SRCS
    systemclipboard.cpp
    waylandclipboard.cpp
)

ecm_add_qtwayland_client_protocol(systemclipboard_SRCS
    PROTOCOL  wlr-data-control-unstable-v1.xml
    BASENAME wlr-data-control-unstable-v1
)

add_library(systemclipboard STATIC ${systemclipboard_SRCS})
set_property(TARGET systemclipboard PROPERTY AUTOMOC ON)
target_link_libraries(systemclipboard
                      Qt5::Gui
                      Qt5::WaylandClient
                      Wayland::Client
)
