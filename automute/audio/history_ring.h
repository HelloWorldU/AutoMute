#pragma once
//
// MonoHistoryRing —— 线程安全的"最近 N 个样本"单声道历史环。
// 一个生产者（抓取线程）push，任意线程 snapshot 旁路拷最近若干样本（在线圈人用）。
//
// 为什么不复用 SpscRingBuffer：那是单向流水线——read 破坏性消费、只准一个消费者、
// 会被读空；而这里要"旁路偷看最近 N 秒、不影响别人"，读者还是第三条线程。故内部
// 自带 mutex（push 极短、snapshot 低频，竞争可忽略）。详见 docs M4.2 对齐。
//
#include <algorithm>
#include <cstddef>
#include <mutex>
#include <vector>

class MonoHistoryRing {
public:
  // 设容量（样本数，如 5s*sr）并清空。每次启动调一次，保证历史是当次的。
  void reset(size_t capacity) {
    std::lock_guard<std::mutex> lk(mu_);
    buf_.assign(capacity ? capacity : 1, 0.0f);
    write_ = 0;
    filled_ = 0;
  }

  // 追加 n 个样本（一次超容量则只保留尾部 capacity 个）。
  void push(const float* mono, size_t n) {
    std::lock_guard<std::mutex> lk(mu_);
    if (buf_.empty())
      buf_.assign(1, 0.0f);
    const size_t cap = buf_.size();
    if (n >= cap) {
      mono += (n - cap);
      n = cap;
    }
    for (size_t i = 0; i < n; ++i) {
      buf_[write_] = mono[i];
      write_ = (write_ + 1) % cap;
    }
    filled_ = std::min(filled_ + n, cap);
  }

  // 拷出最近 want 个样本（不足则给现有的）到 out；返回实际拷出数。空历史返回 0。
  size_t snapshot(size_t want, std::vector<float>& out) const {
    std::lock_guard<std::mutex> lk(mu_);
    if (filled_ == 0) {
      out.clear();
      return 0;
    }
    const size_t cap = buf_.size();
    if (want > filled_)
      want = filled_;
    out.resize(want);
    // 最近 want 个：从 (write_ - want) 处环形读到 write_。
    const size_t start = (write_ + cap - want) % cap;
    for (size_t i = 0; i < want; ++i)
      out[i] = buf_[(start + i) % cap];
    return want;
  }

  size_t filled() const {
    std::lock_guard<std::mutex> lk(mu_);
    return filled_;
  }

private:
  mutable std::mutex mu_;
  std::vector<float> buf_;
  size_t write_ = 0;
  size_t filled_ = 0;
};
