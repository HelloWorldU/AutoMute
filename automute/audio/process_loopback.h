#pragma once
//
// 进程级 loopback 抓取（Win10 2004+）：只抓【某个进程】的音频，而不是整个设备。
// 这是产品化路线 P2 的核心——抓目标 App 进程，渲染到喇叭，天然无反馈，
// 用户无需虚拟声卡。接口刻意和 LoopbackCapture 对齐，方便替换。
//
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

class ProcessLoopbackCapture {
public:
  using DataCallback =
      std::function<void(const float* samples, uint32_t frames, uint32_t channels)>;

  ProcessLoopbackCapture() = default;
  ~ProcessLoopbackCapture();

  // 抓取指定进程（及其子进程树）的音频。失败返回 false 并填 error()。
  bool initialize(uint32_t pid);

  // 阻塞式抓取循环，直到 keepRunning 变 false。每抓到一块调用 cb。
  void run(const DataCallback& cb, std::atomic<bool>& keepRunning);

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
