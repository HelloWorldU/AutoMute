cmake_minimum_required(VERSION 3.20)
project(AutoMute LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 导出 compile_commands.json，供编辑器语言服务器(cpptools/clangd)实时分析代码。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# M2.1: ONNX Runtime 探针 —— 验证 ORT 能在 MinGW+CMake 下链接并初始化。
set(ORT_DIR ${CMAKE_SOURCE_DIR}/third_party/onnxruntime)
if(EXISTS ${ORT_DIR})
  add_executable(automute_ort_probe automute/ort_probe.cpp)
  target_include_directories(automute_ort_probe PRIVATE ${ORT_DIR}/include)
  target_link_libraries(automute_ort_probe PRIVATE ${ORT_DIR}/lib/onnxruntime.lib)
  # 运行时需要 DLL 在 exe 旁边。
  add_custom_command(TARGET automute_ort_probe POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${ORT_DIR}/lib/onnxruntime.dll $<TARGET_FILE_DIR:automute_ort_probe>)
endif()

# M2.2b: 特征提取探针 —— 读 wav（→ fbank → 声纹，逐步长大）。
add_executable(automute_feat_probe
    automute/feat_probe.cpp
    automute/audio/wav_io.cpp
)
target_include_directories(automute_feat_probe PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/dr_libs)
