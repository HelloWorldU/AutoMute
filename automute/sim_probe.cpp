//
// M2.3 验证：算多段语音的声纹，打印余弦相似度矩阵，判定"同人 > 异人"。
// 这是整套声纹方案能否成立的关键证明。
//
//   用法: automute_sim_probe [model.onnx] wav1 wav2 wav3 ...
//   默认用 models/test_speakers 下的 3 段（同人×2 + 异人×1）。
//
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

#include "automute/audio/embedder.h"

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);

  std::string model = "models/voxceleb_ECAPA512_LM.onnx";
  std::vector<std::string> wavs;
  // 简单参数解析：以 .onnx 结尾的当模型，其余当 wav。
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a.size() > 5 && a.substr(a.size() - 5) == ".onnx")
      model = a;
    else
      wavs.push_back(a);
  }
  if (wavs.empty()) {
    wavs = {"models/test_speakers/spkA_1272_1.wav",
            "models/test_speakers/spkA_1272_2.wav",
            "models/test_speakers/spkB_2277.wav"};
  }

  Embedder emb;
  std::string err;
  if (!emb.load(model, err)) {
    printf("%s\n", err.c_str());
    return 1;
  }

  // 算每段的声纹
  std::vector<std::vector<float>> embs;
  for (auto& w : wavs) {
    std::vector<float> e;
    if (!emb.embedWav(w, e, err)) {
      printf("[%s] %s\n", w.c_str(), err.c_str());
      return 1;
    }
    printf("已算声纹: %s  (%zu 维)\n", w.c_str(), e.size());
    embs.push_back(std::move(e));
  }

  // 余弦相似度矩阵
  printf("\n余弦相似度矩阵（越接近 1 越像同一人）:\n      ");
  for (size_t j = 0; j < wavs.size(); ++j)
    printf("  [%zu]  ", j);
  printf("\n");
  for (size_t i = 0; i < embs.size(); ++i) {
    printf("  [%zu] ", i);
    for (size_t j = 0; j < embs.size(); ++j)
      printf(" %.3f ", cosineSimilarity(embs[i], embs[j]));
    printf("  %s\n", wavs[i].c_str());
  }
  return 0;
}
