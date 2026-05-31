cmake_minimum_required(VERSION 3.20)
project(AutoMute LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Headers resolve as "automute/audio/..." from the project root.
include_directories(${CMAKE_SOURCE_DIR})

# WASAPI / COM libraries shared by all targets.
set(WASAPI_LIBS ole32 oleaut32 winmm ksuser uuid)

# M1 step 1: WASAPI loopback capture — listen to system audio, print RMS level.
add_executable(automute_probe
    automute/main.cpp
    automute/audio/loopback_capture.cpp
)
target_link_libraries(automute_probe PRIVATE ${WASAPI_LIBS})

# M1 step 2: capture -> ring buffer -> render passthrough loop, with mute toggle.
add_executable(automute_passthrough
    automute/passthrough_main.cpp
    automute/audio/loopback_capture.cpp
    automute/audio/render_playback.cpp
)
target_link_libraries(automute_passthrough PRIVATE ${WASAPI_LIBS})
