//
// M2.2b 探针（逐步长大）：
//   ① 当前：读 wav → 打印基本信息，验证 wav 读取通。
//   ② 之后：算 fbank 特征。
//   ③ 之后：喂模型 → 真 embedding。
//
#include <cstdio>

#include <windows.h>

#include "automute/audio/wav_io.h"

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);
  const char* path = (argc > 1) ? argv[1] : "models/test_16k_mono.wav";

  std::vector<float> samples;
  uint32_t sr = 0;
  std::string err;
  if (!loadWavMono(path, samples, sr, err)) {
    printf("读取失败: %s\n", err.c_str());
    return 1;
  }

  printf("读取成功: %s\n", path);
  printf("  采样率: %u Hz%s\n", sr, sr == 16000 ? "" : "  ⚠️ 模型要 16kHz，后续需重采样");
  printf("  样本数: %zu\n", samples.size());
  printf("  时长:   %.2f 秒\n", samples.size() / (double)sr);
  if (samples.size() >= 5)
    printf("  前5个样本: %.4f %.4f %.4f %.4f %.4f\n", samples[0], samples[1],
           samples[2], samples[3], samples[4]);
  return 0;
}
