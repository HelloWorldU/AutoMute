#pragma once
//
// WASAPI Loopback 抓取：监听“系统正在播放的混合音频”，把每一小块 PCM 交给回调。
// 这是 AutoMute 的输入端。M1 第 1 步只用它打印音量，验证“能听见系统声音”。
//
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

class LoopbackCapture {
public:
  // 回调拿到的是交错(interleaved) float32 样本：[L,R,L,R,...]，范围约 [-1,1]。
  // frames = 这一块有多少帧；channels = 每帧几个声道。
  using DataCallback = std::function<void(const float* samples, uint32_t frames,
                                          uint32_t channels)>;

  LoopbackCapture() = default;
  ~LoopbackCapture();

  // 初始化 COM、拿输出设备、以 loopback 模式打开。失败返回 false 并填 error()。
  // deviceIndex: -1 用默认设备；>=0 用枚举列表里第 index 个（见 audio_devices）。
  bool initialize(int deviceIndex = -1);

  // 阻塞式抓取循环，直到 keepRunning 变 false。每抓到一块就调用 cb。
  void run(const DataCallback& cb, std::atomic<bool>& keepRunning);

  uint32_t sampleRate() const { return sampleRate_; }
  uint32_t channels() const { return channels_; }
  const std::string& error() const { return error_; }

private:
  void fail(const std::string& where, long hr);

  uint32_t sampleRate_ = 0;
  uint32_t channels_ = 0;
  bool isFloat_ = false;       // mix format 是否为 IEEE float
  uint32_t bitsPerSample_ = 0; // 非 float 时用于转换
  std::string error_;

  // COM 对象用 void* 持有，避免在头文件里拖入
  // <audioclient.h>（编译更快、耦合更低）。
  struct Impl;
  Impl* impl_ = nullptr;
};
