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
    )
else()
    find_package(QtWaylandScanner REQUIRED)
    ecm_add_qtwayland_client_protocol(systemclipboard_SRCS
        PROTOCOL wlr-data-control-unstable-v1.xml
        BASENAME wlr-data-control-unstable-v1
    )
    add_library(systemclipboard STATIC ${systemclipboard_SRCS})
endif()

target_link_libraries(systemclipboard
                      ${copyq_qt}::Gui
                      ${copyq_qt}::WaylandClient
                      Wayland::Client
)
