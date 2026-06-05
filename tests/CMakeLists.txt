# 后端 C++ 测试（doctest + ctest）。详见 docs/testing.md。
#   跑：cmake --build build && ctest --test-dir build --output-on-failure
set(DOCTEST_INC ${CMAKE_SOURCE_DIR}/third_party)

# ── Tier 1 单元（纯逻辑，无 ONNX，快） ──
add_executable(automute_unit_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/ring_buffer_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/history_ring_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit/resampler_test.cpp
    ${CMAKE_SOURCE_DIR}/automute/audio/resampler.cpp)
target_include_directories(automute_unit_tests PRIVATE ${CMAKE_SOURCE_DIR} ${DOCTEST_INC})
add_test(NAME unit COMMAND automute_unit_tests)

# ── Tier 2 集成（声纹管线，链 ONNX/kaldi；仅在主依赖齐时） ──
if(TARGET kaldi-native-fbank-core)
  add_executable(automute_embedder_tests
      ${CMAKE_CURRENT_SOURCE_DIR}/integration/embedder_test.cpp
      ${CMAKE_SOURCE_DIR}/automute/audio/embedder.cpp
      ${CMAKE_SOURCE_DIR}/automute/audio/wav_io.cpp
      ${CMAKE_SOURCE_DIR}/automute/audio/resampler.cpp)
  target_include_directories(automute_embedder_tests PRIVATE
      ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party/dr_libs
      ${ORT_DIR}/include ${KNF_DIR} ${DOCTEST_INC})
  target_link_libraries(automute_embedder_tests PRIVATE
      kaldi-native-fbank-core ${ORT_DIR}/lib/onnxruntime.lib)
  target_compile_definitions(automute_embedder_tests PRIVATE
      AUTOMUTE_ROOT="${CMAKE_SOURCE_DIR}")
  add_custom_command(TARGET automute_embedder_tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${ORT_DIR}/lib/onnxruntime.dll $<TARGET_FILE_DIR:automute_embedder_tests>)
  add_test(NAME embedder COMMAND automute_embedder_tests)
endif()
