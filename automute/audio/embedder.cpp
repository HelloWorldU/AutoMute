#include "automute/audio/embedder.h"
#include "automute/audio/wav_io.h"

#include <cmath>
#include <memory>

#include "kaldi-native-fbank/csrc/online-feature.h"
#include "onnxruntime_cxx_api.h"

static std::wstring widen(const std::string& s) {
  return std::wstring(s.begin(), s.end());
}

struct Embedder::Impl {
  Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "automute"};
  std::unique_ptr<Ort::Session> session;
  Ort::AllocatorWithDefaultOptions alloc;
  std::string inName, outName;
};

Embedder::Embedder() : impl_(new Impl()) {}
Embedder::~Embedder() { delete impl_; }

bool Embedder::load(const std::string& modelPath, std::string& err) {
  try {
    Ort::SessionOptions so;
    impl_->session = std::make_unique<Ort::Session>(
        impl_->env, widen(modelPath).c_str(), so);
    impl_->inName =
        impl_->session->GetInputNameAllocated(0, impl_->alloc).get();
    impl_->outName =
        impl_->session->GetOutputNameAllocated(0, impl_->alloc).get();
    return true;
  } catch (const std::exception& e) {
    err = std::string("加载模型失败: ") + e.what();
    return false;
  }
}

bool Embedder::embed(const std::vector<float>& mono, uint32_t sampleRate,
                     std::vector<float>& out, std::string& err) {
  if (!impl_->session) {
    err = "模型未加载";
    return false;
  }

  // 1) fbank（kaldi 风格，80 维）
  knf::FbankOptions opts;
  opts.frame_opts.samp_freq = (float)sampleRate;
  opts.frame_opts.dither = 0.0f;
  opts.frame_opts.frame_length_ms = 25.0f;
  opts.frame_opts.frame_shift_ms = 10.0f;
  opts.mel_opts.num_bins = 80;
  knf::OnlineFbank fbank(opts);

  std::vector<float> scaled(mono.size());
  for (size_t i = 0; i < mono.size(); ++i)
    scaled[i] = mono[i] * 32768.0f; // kaldi 约定 int16 量级
  fbank.AcceptWaveform((float)sampleRate, scaled.data(), (int32_t)scaled.size());
  fbank.InputFinished();

  const int T = fbank.NumFramesReady();
  const int D = fbank.Dim();
  if (T == 0) {
    err = "音频太短，没算出帧";
    return false;
  }

  std::vector<float> feats((size_t)T * D);
  for (int t = 0; t < T; ++t) {
    const float* f = fbank.GetFrame(t);
    for (int d = 0; d < D; ++d)
      feats[(size_t)t * D + d] = f[d];
  }

  // 2) 均值归一化（每维减时间均值，wespeaker 约定）
  for (int d = 0; d < D; ++d) {
    double mean = 0;
    for (int t = 0; t < T; ++t)
      mean += feats[(size_t)t * D + d];
    mean /= T;
    for (int t = 0; t < T; ++t)
      feats[(size_t)t * D + d] -= (float)mean;
  }

  // 3) 推理
  try {
    std::vector<int64_t> shape = {1, T, D};
    auto mem =
        Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input = Ort::Value::CreateTensor<float>(
        mem, feats.data(), feats.size(), shape.data(), shape.size());
    const char* inN = impl_->inName.c_str();
    const char* outN = impl_->outName.c_str();
    auto outputs = impl_->session->Run(Ort::RunOptions{nullptr}, &inN, &input, 1,
                                       &outN, 1);
    const float* emb = outputs[0].GetTensorMutableData<float>();
    const int dim =
        (int)outputs[0].GetTensorTypeAndShapeInfo().GetShape().back();
    out.assign(emb, emb + dim);
    return true;
  } catch (const std::exception& e) {
    err = std::string("推理失败: ") + e.what();
    return false;
  }
}

bool Embedder::embedWav(const std::string& wavPath, std::vector<float>& out,
                        std::string& err) {
  std::vector<float> mono;
  uint32_t sr = 0;
  if (!loadWavMono(wavPath, mono, sr, err))
    return false;
  return embed(mono, sr, out, err);
}

float cosineSimilarity(const std::vector<float>& a,
                       const std::vector<float>& b) {
  double dot = 0, na = 0, nb = 0;
  size_t n = a.size() < b.size() ? a.size() : b.size();
  for (size_t i = 0; i < n; ++i) {
    dot += (double)a[i] * b[i];
    na += (double)a[i] * a[i];
    nb += (double)b[i] * b[i];
  }
  if (na == 0 || nb == 0)
    return 0.0f;
  return (float)(dot / (std::sqrt(na) * std::sqrt(nb)));
}
