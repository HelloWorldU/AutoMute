// MonoHistoryRing 单元测试（快照"取最近 N 秒"的回绕逻辑——曾是 _tmp_ringtest）。
#include "doctest/doctest.h"

#include "automute/audio/history_ring.h"

#include <algorithm>
#include <vector>

// 参考实现：环只留最近 cap 个，再取其尾部 want 个。
static std::vector<float> refTail(const std::vector<float>& all, size_t want,
                                  size_t cap) {
  size_t filled = std::min(all.size(), cap);
  size_t w = std::min(want, filled);
  return std::vector<float>(all.end() - w, all.end());
}

TEST_CASE("取最近 N 在多批回绕下与参考实现一致") {
  const size_t cap = 100;
  MonoHistoryRing h;
  h.reset(cap);
  std::vector<float> all;
  size_t batches[] = {7, 30, 5, 60, 13, 90, 1, 44, 100, 150};
  float v = 0;
  for (size_t b : batches) {
    std::vector<float> chunk(b);
    for (size_t i = 0; i < b; ++i)
      chunk[i] = v++;
    h.push(chunk.data(), b);
    all.insert(all.end(), chunk.begin(), chunk.end());
    for (size_t want : {(size_t)0, (size_t)1, (size_t)33, (size_t)99,
                        (size_t)100, (size_t)250}) {
      std::vector<float> got;
      h.snapshot(want, got);
      CHECK(got == refTail(all, want, cap));
    }
  }
}

TEST_CASE("空历史 snapshot 返回 0、out 清空") {
  MonoHistoryRing h;
  h.reset(50);
  std::vector<float> o;
  CHECK(h.snapshot(10, o) == 0);
  CHECK(o.empty());
}

TEST_CASE("单次 push 超容量只留尾部 cap 个") {
  MonoHistoryRing h;
  h.reset(10);
  std::vector<float> big(25);
  for (int i = 0; i < 25; ++i)
    big[i] = (float)i;
  h.push(big.data(), 25);
  std::vector<float> got;
  CHECK(h.snapshot(10, got) == 10);
  CHECK(got.front() == 15.0f); // 尾部 10 个 = 15..24
  CHECK(got.back() == 24.0f);
}

TEST_CASE("reset 清空已有历史") {
  MonoHistoryRing h;
  h.reset(8);
  std::vector<float> a = {1, 2, 3};
  h.push(a.data(), 3);
  CHECK(h.filled() == 3);
  h.reset(8);
  CHECK(h.filled() == 0);
}
