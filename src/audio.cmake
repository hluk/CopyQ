find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h
    HINTS "${MINIAUDIO_INCLUDE_DIR}"
    "$ENV{MINIAUDIO_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

if (NOT MINIAUDIO_INCLUDE_DIR OR NOT EXISTS "${MINIAUDIO_INCLUDE_DIR}/miniaudio.h")
    message(FATAL_ERROR
        "Header file miniaudio.h not found. To solve this do any of these:"
        "\na) Install miniaudio development package"
        " (miniaudio-devel on Fedora, libminiaudio-dev on Debian/Ubuntu)"
        "\nb) Download it from https://github.com/mackron/miniaudio"
        " and set MINIAUDIO_INCLUDE_DIR to the directory containing miniaudio.h"
        "\nc) Set WITH_AUDIO=OFF to disable audio support")
endif()
message(STATUS "miniaudio found: ${MINIAUDIO_INCLUDE_DIR}")

# miniaudio_impl.cpp lives in src/miniaudio/ — a separate CMake directory
# scope where CMAKE_CXX_FLAGS is stripped of coverage flags (--coverage
# cannot be negated by -fno-*).  See src/miniaudio/CMakeLists.txt.
add_subdirectory(miniaudio)
set(MINIAUDIO_OBJECTS $<TARGET_OBJECTS:miniaudio_impl>)

list(APPEND copyq_DEFINITIONS WITH_AUDIO)

# miniaudio needs threads and math on POSIX, dl on Linux
find_package(Threads REQUIRED)
if (UNIX AND NOT APPLE)
    list(APPEND copyq_LIBRARIES Threads::Threads m ${CMAKE_DL_LIBS})
elseif (APPLE)
    find_library(COREAUDIO_LIB CoreAudio REQUIRED)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox REQUIRED)
    find_library(COREFOUNDATION_LIB CoreFoundation REQUIRED)
    list(APPEND copyq_LIBRARIES
        ${COREAUDIO_LIB} ${AUDIOTOOLBOX_LIB} ${COREFOUNDATION_LIB})
endif()
# Windows: miniaudio uses WASAPI via COM, linked automatically
