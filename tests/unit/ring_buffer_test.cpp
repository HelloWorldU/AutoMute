// 无锁 SPSC 环形缓冲单元测试（管线命脉）。本 TU 提供 doctest main。
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "automute/audio/ring_buffer.h"

#include <vector>

TEST_CASE("基本写读：写入即可原样读回") {
  SpscRingBuffer<float> rb(8);
  float in[5] = {1, 2, 3, 4, 5};
  CHECK(rb.write(in, 5) == 5);
  CHECK(rb.size() == 5);
  float out[5] = {0};
  CHECK(rb.read(out, 5) == 5);
  for (int i = 0; i < 5; ++i)
    CHECK(out[i] == in[i]);
  CHECK(rb.size() == 0);
}

TEST_CASE("写超容量：只写下可容纳的 N 个") {
  SpscRingBuffer<int> rb(4); // 可用容量 = 构造参数 4
  int in[6] = {1, 2, 3, 4, 5, 6};
  CHECK(rb.write(in, 6) == 4);
  int out[6] = {0};
  CHECK(rb.read(out, 6) == 4);
  CHECK(out[0] == 1);
  CHECK(out[3] == 4);
}

TEST_CASE("回绕：交替写读越过数组末尾，值仍有序不乱") {
  SpscRingBuffer<int> rb(4);
  int v = 0;
  std::vector<int> got;
  for (int round = 0; round < 12; ++round) {
    int w[3] = {v, v + 1, v + 2};
    v += 3;
    size_t n = rb.write(w, 3);
    int r[3] = {0};
    size_t m = rb.read(r, n);
    for (size_t i = 0; i < m; ++i)
      got.push_back(r[i]);
  }
  for (size_t i = 0; i < got.size(); ++i)
    CHECK(got[i] == (int)i);
}

TEST_CASE("空读返回 0") {
  SpscRingBuffer<int> rb(4);
  int out[4] = {0};
  CHECK(rb.read(out, 4) == 0);
  CHECK(rb.size() == 0);
}
