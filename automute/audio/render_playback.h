#pragma once
//
// WASAPI 渲染回放：从环形缓冲取音频，写回默认输出设备。
// 这是 AutoMute 的输出端，与 LoopbackCapture(输入端)对称。
//
// 采用【事件驱动】模式：不轮询，而是让音频引擎在“缓冲快空了、需要喂数据”时
// 通过一个 Win32 事件唤醒渲染线程。这是低延迟回放的标准做法。
//
#include <atomic>
#include <cstdint>
#include <string>

#include "automute/audio/ring_buffer.h"

class RenderPlayback {
public:
  RenderPlayback() = default;
  ~RenderPlayback();

  // 在【渲染线程】上调用：初始化 COM、拿默认输出设备、以事件驱动模式打开。
  bool initialize();

  // 阻塞式渲染循环：被事件唤醒 → 从 src 取数据填进设备缓冲 → 直到 keepRunning
  // 变 false。 src 不够时(欠载/underrun)用静音补齐，避免爆音。
  // muted 非空且为 true 时输出静音（声纹判定驱动的自动掐声）。
  void run(SpscRingBuffer<float>& src, std::atomic<bool>& keepRunning,
           std::atomic<bool>* muted = nullptr);

  uint32_t sampleRate() const { return sampleRate_; }
  uint32_t channels() const { return channels_; }
  const std::string& error() const { return error_; }

private:
  void fail(const std::string& where, long hr);

  uint32_t sampleRate_ = 0;
  uint32_t channels_ = 0;
  std::string error_;

  struct Impl;
  Impl* impl_ = nullptr;
};
