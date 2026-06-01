#pragma once
//
// 无锁 SPSC 环形缓冲 (Single-Producer / Single-Consumer ring buffer)
// ───────────────────────────────────────────────────────────────────
// 实时音频的命脉。抓取线程(生产者)往里写，渲染线程(消费者)从里读，
// 两条线程不用任何锁就能安全协作。
//
// 为什么必须无锁？
//   渲染线程被音频驱动以固定节奏唤醒（比如每 10ms 要交出一块数据）。
//   它一旦阻塞——等锁、分配内存、缺页——就会“欠交”，你就听到爆音(glitch)。
//   所以实时音频铁律：渲染回调里【不加锁、不 new、不调用任何可能阻塞的东西】。
//
// SPSC 的“S”是关键约束：只有【一条】生产者线程、【一条】消费者线程。
//   正因为各只有一条，才能用两个原子下标无锁同步；多生产者就得另想办法。
//
// 核心原理：一个数组 + 两个下标(head 写到哪了 / tail 读到哪了)，下标只增不减，
//   用容量取模回绕成“环”。head==tail 表示空；写满前留一个空位区分“空”与“满”。
//
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <vector>

template <typename T> class SpscRingBuffer {
public:
  // capacity = 能存多少个元素。实际多分配 1 个空位用于区分“空/满”。
  explicit SpscRingBuffer(size_t capacity)
      : buffer_(capacity + 1), capacity_(capacity + 1) {}

  // 生产者调用：写入 n 个元素，返回【实际】写入数（缓冲快满时可能少于 n）。
  size_t write(const T* data, size_t n) {
    // 自己的下标(head)用 relaxed 读即可——只有本线程改它。
    const size_t head = head_.load(std::memory_order_relaxed);
    // 对方的下标(tail)必须 acquire 读：确保看到消费者“已读走”的最新进度。
    const size_t tail = tail_.load(std::memory_order_acquire);

    const size_t freeSpace = freeCount(head, tail);
    if (n > freeSpace)
      n = freeSpace;

    // 可能要跨越数组末尾，分两段拷贝。
    const size_t firstChunk = std::min(n, capacity_ - head);
    std::memcpy(&buffer_[head], data, firstChunk * sizeof(T));
    if (n > firstChunk)
      std::memcpy(&buffer_[0], data + firstChunk, (n - firstChunk) * sizeof(T));

    // release 存储：保证上面的数据写入，对随后 acquire 读到新 head
    // 的消费者【可见】。
    head_.store(advance(head, n), std::memory_order_release);
    return n;
  }

  // 消费者调用：读出最多 n 个元素，返回【实际】读出数（缓冲快空时可能少于 n）。
  size_t read(T* out, size_t n) {
    const size_t tail = tail_.load(std::memory_order_relaxed);
    const size_t head = head_.load(std::memory_order_acquire);

    const size_t available = usedCount(head, tail);
    if (n > available)
      n = available;

    const size_t firstChunk = std::min(n, capacity_ - tail);
    std::memcpy(out, &buffer_[tail], firstChunk * sizeof(T));
    if (n > firstChunk)
      std::memcpy(out + firstChunk, &buffer_[0], (n - firstChunk) * sizeof(T));

    tail_.store(advance(tail, n), std::memory_order_release);
    return n;
  }

  // 当前可读元素数（消费者视角的近似快照）。
  size_t size() const {
    return usedCount(head_.load(std::memory_order_acquire),
                     tail_.load(std::memory_order_acquire));
  }

private:
  size_t advance(size_t idx, size_t n) const { return (idx + n) % capacity_; }

  size_t usedCount(size_t head, size_t tail) const {
    return (head + capacity_ - tail) % capacity_;
  }
  size_t freeCount(size_t head, size_t tail) const {
    // 留 1 个空位，所以可用空间是 capacity_-1 减去已用。
    return capacity_ - 1 - usedCount(head, tail);
  }

  std::vector<T> buffer_;
  const size_t capacity_;
  // 两个下标分别只被一条线程“写”，故无需锁；内存序保证彼此可见。
  std::atomic<size_t> head_{0}; // 生产者写、消费者读
  std::atomic<size_t> tail_{0}; // 消费者写、生产者读
};
