#include "automute/audio/resampler.h"

#include <cmath>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

double sinc(double x) {
  if (std::abs(x) < 1e-9)
    return 1.0;
  return std::sin(kPi * x) / (kPi * x);
}

// 构造一次 48k→16k 用的低通 FIR（截止 ~7.6kHz，Hamming 窗），归一化到增益 1。
const std::vector<float>& fir48to16() {
  static const std::vector<float> h = [] {
    const int M = 48;             // 阶数（49 抽头）
    const double fs = 48000.0;
    const double fc = 7600.0;     // 截止，留点过渡带到 8k
    std::vector<float> taps(M + 1);
    double sum = 0;
    for (int n = 0; n <= M; ++n) {
      double t = (double)n - M / 2.0;
      double lp = (2 * fc / fs) * sinc(2 * fc / fs * t);
      double w = 0.54 - 0.46 * std::cos(2 * kPi * n / M); // Hamming
      taps[n] = (float)(lp * w);
      sum += taps[n];
    }
    for (auto& v : taps)
      v = (float)(v / sum); // 归一化
    return taps;
  }();
  return h;
}

} // namespace

void resampleTo16k(const std::vector<float>& in, uint32_t inRate,
                   std::vector<float>& out) {
  if (inRate == 16000) {
    out = in;
    return;
  }

  if (inRate == 48000) {
    const auto& h = fir48to16();
    const int M = (int)h.size() - 1;
    const int n = (int)in.size();
    out.clear();
    out.reserve(n / 3 + 1);
    // 每隔 3 个采样取一个输出，在该位置应用居中的 FIR。
    for (int c = 0; c < n; c += 3) {
      double acc = 0;
      for (int k = 0; k <= M; ++k) {
        int idx = c + k - M / 2;
        if (idx >= 0 && idx < n)
          acc += (double)h[k] * in[idx];
      }
      out.push_back((float)acc);
    }
    return;
  }

  // 退路：线性插值到 16k（非 48k 的少见情况，如 44.1k）。
  const double ratio = 16000.0 / (double)inRate;
  const int outN = (int)(in.size() * ratio);
  out.resize(outN);
  for (int i = 0; i < outN; ++i) {
    double srcPos = i / ratio;
    int i0 = (int)srcPos;
    double frac = srcPos - i0;
    float a = in[i0];
    float b = (i0 + 1 < (int)in.size()) ? in[i0 + 1] : a;
    out[i] = (float)(a + (b - a) * frac);
  }
}
