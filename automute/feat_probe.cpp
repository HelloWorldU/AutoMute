//
// M2.2b 完整管线：wav → fbank 特征 → ONNX 声纹模型 → embedding 向量。
//   读 wav（dr_wav）→ 算 80 维 kaldi fbank（kaldi-native-fbank）
//   → 均值归一化（wespeaker 约定）→ 喂模型 → 打印 192 维声纹。
//
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

#include "automute/audio/wav_io.h"
#include "kaldi-native-fbank/csrc/online-feature.h"
#include "onnxruntime_cxx_api.h"

static std::wstring widen(const std::string& s) {
  return std::wstring(s.begin(), s.end());
}

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);
  std::string wavPath = (argc > 1) ? argv[1] : "models/test_16k_mono.wav";
  std::string modelPath =
      (argc > 2) ? argv[2] : "models/voxceleb_ECAPA512_LM.onnx";

  // 1) 读 wav → 单声道 float
  std::vector<float> samples;
  uint32_t sr = 0;
  std::string err;
  if (!loadWavMono(wavPath, samples, sr, err)) {
    printf("读 wav 失败: %s\n", err.c_str());
    return 1;
  }
  printf("wav: %u Hz, %zu 样本, %.2fs\n", sr, samples.size(),
         samples.size() / (double)sr);

  // 2) 算 fbank（kaldi 风格，80 维）
  knf::FbankOptions opts;
  opts.frame_opts.samp_freq = (float)sr;
  opts.frame_opts.dither = 0.0f; // 关抖动 → 确定性输出
  opts.frame_opts.frame_length_ms = 25.0f;
  opts.frame_opts.frame_shift_ms = 10.0f;
  opts.mel_opts.num_bins = 80;
  knf::OnlineFbank fbank(opts);

  // kaldi 约定输入为 int16 量级，把 [-1,1] 放大 32768（均值归一后其实无所谓，
  // 但按惯例对齐）。
  std::vector<float> scaled(samples.size());
  for (size_t i = 0; i < samples.size(); ++i)
    scaled[i] = samples[i] * 32768.0f;
  fbank.AcceptWaveform((float)sr, scaled.data(), (int32_t)scaled.size());
  fbank.InputFinished();

  const int T = fbank.NumFramesReady();
  const int D = fbank.Dim();
  printf("fbank: %d 帧 x %d 维\n", T, D);
  if (T == 0) {
    printf("音频太短，没算出帧\n");
    return 1;
  }

  std::vector<float> feats((size_t)T * D);
  for (int t = 0; t < T; ++t) {
    const float* f = fbank.GetFrame(t);
    for (int d = 0; d < D; ++d)
      feats[(size_t)t * D + d] = f[d];
  }

  // 3) 均值归一化：每个 mel 维减去其在时间上的均值（wespeaker 约定）
  for (int d = 0; d < D; ++d) {
    double mean = 0;
    for (int t = 0; t < T; ++t)
      mean += feats[(size_t)t * D + d];
    mean /= T;
    for (int t = 0; t < T; ++t)
      feats[(size_t)t * D + d] -= (float)mean;
  }

  // 4) 喂模型 → embedding
  Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "automute");
  Ort::SessionOptions so;
  Ort::Session session(env, widen(modelPath).c_str(), so);
  Ort::AllocatorWithDefaultOptions alloc;
  auto inName = session.GetInputNameAllocated(0, alloc);
  auto outName = session.GetOutputNameAllocated(0, alloc);

  std::vector<int64_t> shape = {1, T, D};
  auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value input = Ort::Value::CreateTensor<float>(
      mem, feats.data(), feats.size(), shape.data(), shape.size());

  const char* inN = inName.get();
  const char* outN = outName.get();
  auto outputs =
      session.Run(Ort::RunOptions{nullptr}, &inN, &input, 1, &outN, 1);

  float* emb = outputs[0].GetTensorMutableData<float>();
  const int dim = (int)outputs[0].GetTensorTypeAndShapeInfo().GetShape().back();

  double norm = 0;
  for (int i = 0; i < dim; ++i)
    norm += (double)emb[i] * emb[i];
  norm = std::sqrt(norm);

  printf("✅ 声纹 embedding: %d 维, L2范数=%.3f\n", dim, norm);
  printf("   前6维: ");
  for (int i = 0; i < 6 && i < dim; ++i)
    printf("%.4f ", emb[i]);
  printf("\n");
  return 0;
}
