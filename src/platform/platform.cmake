if (X11_FOUND)
    include(platform/x11/x11platform.cmake)
elseif (Q_WS_WIN)
    include(platform/win/winplatform.cmake)
elseif (Q_WS_MAC)
    include(platform/mac/macplatform.cmake)
else()
    file(GLOB copyq_SOURCES ${copyq_SOURCES}
        platform/dummy/*.cpp
        )
    set(USE_QXT FALSE)
endif()

