#pragma once
//
// AutoMute 引擎：进程 loopback 抓取 → 声纹门控 → 渲染，与界面解耦。
// CLI / 未来的 GUI 都驱动它：
//   prepare(cfg) 登记目标声纹 → start(pid) 跑抓取/渲染/分析三线程 →
//   轮询 similarity()/muted() 显示状态 → stop()。
// 引擎本身【不做任何 UI 输出】（printf/键盘都留在前端），方便换壳。
//
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "automute/audio/embedder.h"
#include "automute/audio/ring_buffer.h"

class AutoMuteEngine {
public:
  // 运行参数。前端（CLI/GUI）填好后交给 prepare()。
  struct Config {
    std::string model = "models/voxceleb_ECAPA512_LM.onnx";
    std::string targetWav = "models/test_speakers/spkA_1272_1.wav";
    float threshold = 0.5f; // 相似度 > 阈值 判为目标 → 掐声
    float windowSec = 1.5f; // 声纹判定窗长（地板≈1.5s，见 M3.4）
    float hopSec = 0.25f;   // 滑动窗每滑过这么久重判一次
  };

  AutoMuteEngine() = default;
  ~AutoMuteEngine();
  AutoMuteEngine(const AutoMuteEngine&) = delete;
  AutoMuteEngine& operator=(const AutoMuteEngine&) = delete;

  // 加载模型 + 登记目标声纹。须在 start 前调一次。失败返回 false 并填 err。
  bool prepare(const Config& cfg, std::string& err);

  // 启动抓取/渲染/分析三线程，对进程 pid 开始处理。失败返回 false 并填 err。
  bool start(uint32_t pid, std::string& err);

  // 停止三线程并 join（可重复调用；析构自动调用）。
  void stop();

  // 线程安全状态查询（UI 轮询用）。
  float similarity() const { return lastSim_.load(); }
  bool muted() const { return muted_.load(); }
  bool running() const { return running_.load(); }
  // 三线程因初始化失败而中止时的原因（running() 变 false 后读取）。
  const std::string& error() const { return error_; }

private:
  Config cfg_;
  Embedder emb_;
  std::vector<float> target_; // 目标声纹

  SpscRingBuffer<float> rbPlay_{48000 * 8}; // 播放：交错立体声
  SpscRingBuffer<float> rbAna_{48000 * 4};  // 分析：单声道
  std::atomic<bool> running_{false};
  std::atomic<bool> muted_{false};
  std::atomic<uint32_t> sampleRate_{48000};
  std::atomic<float> lastSim_{0.0f};
  std::string error_;

  std::thread capThread_, renThread_, anaThread_;
};
