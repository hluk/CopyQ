# Build miniaudio implementation without coverage instrumentation.
#
# GCC's --coverage flag cannot be negated by -fno-profile-arcs or
# -fno-test-coverage — it acts as an independent meta-flag.  The only way
# to exclude a source file from coverage is to compile it in a separate
# directory scope where CMAKE_CXX_FLAGS has been stripped of --coverage
# (and the related -fprofile-* / -ftest-coverage flags).
#
# This matters because coverage instrumentation changes threading behaviour
# inside miniaudio's PulseAudio backend enough to trigger a deadlock in
# pa_mainloop_iterate during ma_engine_init.

# Strip coverage-related flags from this scope's CMAKE_CXX_FLAGS.
string(REPLACE "--coverage"         "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "-fprofile-arcs"     "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "-ftest-coverage"    "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REPLACE "-fprofile-abs-path" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "-fprofile-update=[^ ]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_library(miniaudio_impl OBJECT miniaudio_impl.cpp)
target_include_directories(miniaudio_impl SYSTEM PRIVATE "${MINIAUDIO_INCLUDE_DIR}")

# Suppress all warnings — this is third-party code.
if (MSVC)
    target_compile_options(miniaudio_impl PRIVATE /W0)
else()
    target_compile_options(miniaudio_impl PRIVATE -w)
endif()
