#pragma once
//
// AutoMute 引擎：进程 loopback 抓取 → 声纹门控 → 渲染，与界面解耦。
// CLI / 未来的 GUI 都驱动它：
//   prepare(cfg) 加载模型 → start(pid) 跑抓取/渲染/分析三线程 →
//   轮询 similarity()/muted() 显示状态 → stop()。
// 引擎本身【不做任何 UI 输出】（printf/键盘都留在前端），方便换壳。
//
// M4.2 扩展：
//   ① 多目标——比对一组命名声纹，任一【开了静音开关】的人命中就掐
//     （v1 列表长度=1，但按 N 设计）。运行中可随时 addTarget。
//   ② 最近 N 秒快照——snapshotRecent() 拷出抓取线程维护的 mono 历史，
//     供「边看边圈」在线登记用（详见 docs/exec-plans/m4-app-shell.md 的 M4.2 对齐）。
//
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "automute/audio/embedder.h"
#include "automute/audio/history_ring.h"
#include "automute/audio/ring_buffer.h"

class AutoMuteEngine {
public:
  // 运行参数。前端（CLI/GUI）填好后交给 prepare()。
  struct Config {
    std::string model = "models/voxceleb_ECAPA512_LM.onnx";
    // 启动即登记的目标（CLI 用；GUI 走在线抓取可留空）。空则启动时无目标，
    // 靠 addTarget 在线登记。
    std::string targetWav = "models/test_speakers/spkA_1272_1.wav";
    float threshold = 0.5f; // 相似度 > 阈值 判为目标 → 掐声
    float windowSec = 1.5f; // 声纹判定窗长（地板≈1.5s，见 M3.4）
    float hopSec = 0.25f;   // 滑动窗每滑过这么久重判一次
  };

  // 单个目标的对外快照（UI 画每人仪表用）。
  struct TargetView {
    std::string name;
    float similarity; // 最近一次比对的相似度
    bool muted;       // 该目标的静音开关是否开着
  };

  AutoMuteEngine() = default;
  ~AutoMuteEngine();
  AutoMuteEngine(const AutoMuteEngine&) = delete;
  AutoMuteEngine& operator=(const AutoMuteEngine&) = delete;

  // 加载模型（+ 若 cfg.targetWav 非空，登记为首个目标并开静音开关，保持 CLI 旧行为）。
  // 须在 start 前调一次。失败返回 false 并填 err。
  bool prepare(const Config& cfg, std::string& err);

  // 启动抓取/渲染/分析三线程，对进程 pid 开始处理。失败返回 false 并填 err。
  // 允许零目标启动（之后用 addTarget 在线登记）。
  bool start(uint32_t pid, std::string& err);

  // 停止三线程并 join（可重复调用；析构自动调用）。
  void stop();

  // ── 多目标管理（线程安全，运行中可调） ──
  // 从单声道样本登记一个命名目标；静音开关【默认关】。返回目标索引，失败返回 -1。
  int addTarget(const std::string& name, const std::vector<float>& mono,
                uint32_t sr, std::string& err);
  // 便捷：直接从 wav 登记。
  int addTargetFromWav(const std::string& name, const std::string& wavPath,
                       std::string& err);
  // 切换某目标的静音开关（idx 越界忽略）。
  void setTargetMuted(size_t idx, bool on);
  // 改名 / 删除某目标（idx 越界忽略；运行中可调，线程安全）。
  void renameTarget(size_t idx, const std::string& name);
  void removeTarget(size_t idx);
  size_t targetCount() const;
  // 取某目标的对外快照（idx 越界返回空 name 的视图）。
  TargetView targetAt(size_t idx) const;

  // ── 最近 N 秒快照（在线圈人登记的地基） ──
  // 拷出最近 seconds 秒的单声道样本到 outMono，sr 填采样率。运行中随时可调，
  // 不影响分析流水线。历史不足 seconds 就给现有的；无历史返回 false。
  bool snapshotRecent(float seconds, std::vector<float>& outMono,
                      uint32_t& sr) const;

  // ── 线程安全聚合状态查询（UI 轮询用） ──
  float similarity() const { return lastSim_.load(); } // 所有目标中最高的相似度
  bool muted() const { return muted_.load(); } // 任一开了静音的目标命中
  bool running() const { return running_.load(); }
  // 三线程因初始化失败而中止时的原因（running() 变 false 后读取）。
  const std::string& error() const { return error_; }

private:
  // 一个命名目标。muted/lastSim 用原子，getter 与分析线程无锁交互；
  // 增删目标本身受 targetsMu_ 保护。
  struct Target {
    std::string name;
    std::vector<float> emb;
    std::atomic<bool> muted{false};
    std::atomic<float> lastSim{0.0f};
  };

  // 算嵌入已成功后追加目标，返回索引（addTarget/addTargetFromWav 共用）。
  int enroll(const std::string& name, std::vector<float>&& emb);

  Config cfg_;
  Embedder emb_;
  bool prepared_ = false;

  // 目标列表：用 unique_ptr 因 Target 含原子不可移动。支持增/改/删，
  // 全程受 targetsMu_ 保护；分析线程比对时也持锁（cosine 很便宜），
  // 故删除目标不会让分析线程拿到悬空指针。
  mutable std::mutex targetsMu_;
  std::vector<std::unique_ptr<Target>> targets_;

  // 最近 N 秒 mono 历史环（抓取线程写、snapshotRecent 旁路拷）。详见 history_ring.h。
  MonoHistoryRing hist_;
  static constexpr float kHistorySec = 5.0f;

  SpscRingBuffer<float> rbPlay_{48000 * 8}; // 播放：交错立体声
  SpscRingBuffer<float> rbAna_{48000 * 4};  // 分析：单声道
  std::atomic<bool> running_{false};
  std::atomic<bool> muted_{false};
  std::atomic<uint32_t> sampleRate_{48000};
  std::atomic<float> lastSim_{0.0f};
  std::string error_;

  std::thread capThread_, renThread_, anaThread_;
};
