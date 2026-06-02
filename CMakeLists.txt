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

# M2.2b: 特征提取 + 声纹推理探针 —— wav → fbank → ONNX 模型 → embedding。
set(KNF_DIR ${CMAKE_SOURCE_DIR}/third_party/kaldi-native-fbank)
if(EXISTS ${ORT_DIR} AND EXISTS ${KNF_DIR})
  set(KALDI_NATIVE_FBANK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(KALDI_NATIVE_FBANK_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
  add_subdirectory(${KNF_DIR} kaldi-native-fbank-build)

  # 声纹提取的共享源码/依赖（feat_probe 与 sim_probe 都用）。
  set(EMBED_SRCS automute/audio/embedder.cpp automute/audio/wav_io.cpp
                 automute/audio/resampler.cpp)
  set(EMBED_INCS
      ${CMAKE_SOURCE_DIR}/third_party/dr_libs
      ${ORT_DIR}/include
      ${KNF_DIR})  # 让 "kaldi-native-fbank/csrc/..." 解析得到
  set(EMBED_LIBS kaldi-native-fbank-core ${ORT_DIR}/lib/onnxruntime.lib)

  # feat_probe：算单段声纹（M2.2b）
  add_executable(automute_feat_probe automute/feat_probe.cpp ${EMBED_SRCS})
  target_include_directories(automute_feat_probe PRIVATE ${EMBED_INCS})
  target_link_libraries(automute_feat_probe PRIVATE ${EMBED_LIBS})

  # sim_probe：余弦相似度矩阵，验证同人>异人（M2.3）
  add_executable(automute_sim_probe automute/sim_probe.cpp ${EMBED_SRCS})
  target_include_directories(automute_sim_probe PRIVATE ${EMBED_INCS})
  target_link_libraries(automute_sim_probe PRIVATE ${EMBED_LIBS})

  # detect：实时声纹检测（抓取 + 分析线程，M2.4）
  add_executable(automute_detect
      automute/detect_main.cpp ${EMBED_SRCS} automute/audio/loopback_capture.cpp)
  target_include_directories(automute_detect PRIVATE ${EMBED_INCS})
  target_link_libraries(automute_detect PRIVATE ${EMBED_LIBS} ${WASAPI_LIBS})

  # automute_app：主程序 —— 自动定向静音闭环（M2.5）
  add_executable(automute_app
      automute/app_main.cpp ${EMBED_SRCS}
      automute/audio/loopback_capture.cpp automute/audio/render_playback.cpp
      automute/audio/audio_devices.cpp)
  target_include_directories(automute_app PRIVATE ${EMBED_INCS})
  target_link_libraries(automute_app PRIVATE ${EMBED_LIBS} ${WASAPI_LIBS})

  # 都要 onnxruntime.dll 在 exe 旁边。
  foreach(t automute_feat_probe automute_sim_probe automute_detect automute_app)
    add_custom_command(TARGET ${t} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ORT_DIR}/lib/onnxruntime.dll $<TARGET_FILE_DIR:${t}>)
  endforeach()
endif()
