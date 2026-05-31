cmake_minimum_required(VERSION 3.20)
project(AutoMute LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# M1 step 1: WASAPI loopback capture — listen to system audio, print RMS level.
add_executable(automute_probe
    automute/main.cpp
    automute/audio/loopback_capture.cpp
)
# Include root is the project root, so headers resolve as "automute/audio/...".
target_include_directories(automute_probe PRIVATE ${CMAKE_SOURCE_DIR})

# WASAPI / COM libraries
target_link_libraries(automute_probe PRIVATE ole32 oleaut32 winmm ksuser uuid)
