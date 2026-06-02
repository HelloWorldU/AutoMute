//
// M2.2b 探针：wav → 声纹 embedding（现在走 Embedder 封装）。
//   用法: automute_feat_probe [wav] [model.onnx]
//
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

#include "automute/audio/embedder.h"

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);
  std::string wavPath = (argc > 1) ? argv[1] : "models/test_16k_mono.wav";
  std::string modelPath =
      (argc > 2) ? argv[2] : "models/voxceleb_ECAPA512_LM.onnx";

  Embedder emb;
  std::string err;
  if (!emb.load(modelPath, err)) {
    printf("%s\n", err.c_str());
    return 1;
  }

  std::vector<float> e;
  if (!emb.embedWav(wavPath, e, err)) {
    printf("[%s] %s\n", wavPath.c_str(), err.c_str());
    return 1;
  }

  double norm = 0;
  for (float v : e)
    norm += (double)v * v;
  norm = std::sqrt(norm);

  printf("✅ %s\n   声纹 %zu 维, L2范数=%.3f, 前6维: ", wavPath.c_str(), e.size(),
         norm);
  for (size_t i = 0; i < 6 && i < e.size(); ++i)
    printf("%.4f ", e[i]);
  printf("\n");
  return 0;
}
