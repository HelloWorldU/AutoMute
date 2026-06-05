// 重采样 48k→16k 单元测试（长度比例 / DC 保持 / 不产生 NaN）。
#include "doctest/doctest.h"

#include "automute/audio/resampler.h"

#include <cmath>
#include <vector>

static bool allFinite(const std::vector<float>& v) {
  for (float x : v)
    if (!std::isfinite(x))
      return false;
  return true;
}

TEST_CASE("16k 直通：原样返回") {
  std::vector<float> in(100);
  for (int i = 0; i < 100; ++i)
    in[i] = std::sin(i * 0.1f);
  std::vector<float> out;
  resampleTo16k(in, 16000, out);
  CHECK(out.size() == in.size());
  for (size_t i = 0; i < in.size(); ++i)
    CHECK(out[i] == doctest::Approx(in[i]));
}

TEST_CASE("48k→16k：长度约 1/3、DC 保持、全有限") {
  std::vector<float> in(3000, 0.5f); // 纯直流
  std::vector<float> out;
  resampleTo16k(in, 48000, out);
  CHECK((double)out.size() == doctest::Approx(1000).epsilon(0.05));
  CHECK(allFinite(out));
  // 直流不该被低通改变；取中段避开边界暂态。
  double sum = 0;
  int n = 0;
  for (size_t i = out.size() / 4; i < out.size() * 3 / 4; ++i) {
    sum += out[i];
    ++n;
  }
  CHECK(sum / n == doctest::Approx(0.5).epsilon(0.06));
}

TEST_CASE("44100→16k 线性退路：长度约比例、全有限") {
  std::vector<float> in(4410);
  for (int i = 0; i < 4410; ++i)
    in[i] = std::sin(i * 0.05f);
  std::vector<float> out;
  resampleTo16k(in, 44100, out);
  CHECK((double)out.size() ==
        doctest::Approx(4410.0 * 16000 / 44100).epsilon(0.05));
  CHECK(allFinite(out));
}
