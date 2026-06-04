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

# 进程级 loopback 诊断探针（不依赖 ORT/knf）：剥离声纹只测抓取，排查抓取问题用。
add_executable(automute_ploop_probe
    automute/ploop_probe.cpp automute/audio/process_loopback.cpp)
target_link_libraries(automute_ploop_probe PRIVATE ${WASAPI_LIBS} mmdevapi)

# ONNX Runtime 路径（声纹推理依赖；下方 sim_probe/detect/app 用到）。
set(ORT_DIR ${CMAKE_SOURCE_DIR}/third_party/onnxruntime)

# 声纹相关目标 —— wav → fbank → ONNX 模型 → embedding。
set(KNF_DIR ${CMAKE_SOURCE_DIR}/third_party/kaldi-native-fbank)
if(EXISTS ${ORT_DIR} AND EXISTS ${KNF_DIR})
  set(KALDI_NATIVE_FBANK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(KALDI_NATIVE_FBANK_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
  add_subdirectory(${KNF_DIR} kaldi-native-fbank-build)

  # 声纹提取的共享源码/依赖（sim_probe / detect / app 都用）。
  set(EMBED_SRCS automute/audio/embedder.cpp automute/audio/wav_io.cpp
                 automute/audio/resampler.cpp)
  set(EMBED_INCS
      ${CMAKE_SOURCE_DIR}/third_party/dr_libs
      ${ORT_DIR}/include
      ${KNF_DIR})  # 让 "kaldi-native-fbank/csrc/..." 解析得到
  set(EMBED_LIBS kaldi-native-fbank-core ${ORT_DIR}/lib/onnxruntime.lib)

  # sim_probe：余弦相似度矩阵，验证同人>异人（M2.3）；M3 调阈值仍用
  add_executable(automute_sim_probe automute/sim_probe.cpp ${EMBED_SRCS})
  target_include_directories(automute_sim_probe PRIVATE ${EMBED_INCS})
  target_link_libraries(automute_sim_probe PRIVATE ${EMBED_LIBS})

  # detect：实时声纹检测（抓取 + 分析线程，M2.4）
  add_executable(automute_detect
      automute/detect_main.cpp ${EMBED_SRCS} automute/audio/loopback_capture.cpp)
  target_include_directories(automute_detect PRIVATE ${EMBED_INCS})
  target_link_libraries(automute_detect PRIVATE ${EMBED_LIBS} ${WASAPI_LIBS})

  # automute_app：主程序 —— 进程级定向静音（P2.4，进程 loopback，无需虚拟声卡）
  add_executable(automute_app
      automute/app_main.cpp automute/engine.cpp ${EMBED_SRCS}
      automute/audio/process_loopback.cpp automute/audio/render_playback.cpp
      automute/audio/audio_sessions.cpp)
  target_include_directories(automute_app PRIVATE ${EMBED_INCS})
  target_link_libraries(automute_app PRIVATE ${EMBED_LIBS} ${WASAPI_LIBS} mmdevapi)

  # 都要 onnxruntime.dll 在 exe 旁边。
  foreach(t automute_sim_probe automute_detect automute_app)
    add_custom_command(TARGET ${t} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${ORT_DIR}/lib/onnxruntime.dll $<TARGET_FILE_DIR:${t}>)
  endforeach()
endif()
