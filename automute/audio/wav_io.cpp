// dr_wav 是"单头库"：在【唯一一个】.cpp 里定义 IMPLEMENTATION，才会把实现编进来。
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "automute/audio/wav_io.h"

bool loadWavMono(const std::string& path, std::vector<float>& out,
                 uint32_t& sampleRate, std::string& err) {
  unsigned int channels = 0;
  unsigned int sr = 0;
  drwav_uint64 totalFrames = 0;

  // dr_wav 一行读出全部样本，统一解码成 float。
  float* data = drwav_open_file_and_read_pcm_frames_f32(
      path.c_str(), &channels, &sr, &totalFrames, nullptr);
  if (!data) {
    err = "无法打开或解码 wav: " + path;
    return false;
  }

  // 多声道取平均 → 单声道（声纹模型要单声道）。
  out.resize((size_t)totalFrames);
  for (drwav_uint64 i = 0; i < totalFrames; ++i) {
    float s = 0.0f;
    for (unsigned int c = 0; c < channels; ++c)
      s += data[i * channels + c];
    out[(size_t)i] = s / (float)channels;
  }

  sampleRate = sr;
  drwav_free(data, nullptr);
  return true;
}
