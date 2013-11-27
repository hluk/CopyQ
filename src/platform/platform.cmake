# If window system is X11 then set X11_FOUND to TRUE.
find_package(X11)

if (X11_FOUND)
    include(platform/x11/x11platform.cmake)
elseif (Q_WS_WIN OR WIN32)
    include(platform/win/winplatform.cmake)
elseif (Q_OS_MAC OR Q_WS_MAC)
    include(platform/mac/macplatform.cmake)
else()
    message(WARNING "Building on unsupported platfom!")
    file(GLOB copyq_SOURCES ${copyq_SOURCES}
        platform/dummy/*.cpp
        )
    set(USE_QXT FALSE)
endif()

