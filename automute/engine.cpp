#include "automute/engine.h"

#include <chrono>

#include "automute/audio/process_loopback.h"
#include "automute/audio/render_playback.h"

AutoMuteEngine::~AutoMuteEngine() { stop(); }

bool AutoMuteEngine::prepare(const Config& cfg, std::string& err) {
  cfg_ = cfg;
  if (!emb_.load(cfg_.model, err))
    return false;
  if (!emb_.embedWav(cfg_.targetWav, target_, err)) {
    err = "enroll 目标失败: " + err;
    return false;
  }
  return true;
}

bool AutoMuteEngine::start(uint32_t pid, std::string& err) {
  if (running_.load()) {
    err = "引擎已在运行";
    return false;
  }
  if (target_.empty()) {
    err = "请先 prepare() 登记目标声纹";
    return false;
  }
  error_.clear();
  running_.store(true);

  // 抓取线程：进程 loopback → 交错写播放缓冲，降混单声道写分析缓冲。
  // 注：初始化失败时先写 error_ 再 running_.store(false)；前端见 running()==false
  // 后读 error()，靠 running_ 的 release/acquire 保证 error_ 写入可见。
  capThread_ = std::thread([this, pid] {
    ProcessLoopbackCapture cap;
    if (!cap.initialize(pid)) {
      error_ = "进程抓取初始化失败: " + cap.error();
      running_.store(false);
      return;
    }
    sampleRate_.store(cap.sampleRate());
    std::vector<float> mono;
    cap.run(
        [this, &mono](const float* s, uint32_t frames, uint32_t ch) {
          rbPlay_.write(s, (size_t)frames * ch);
          mono.resize(frames);
          for (uint32_t f = 0; f < frames; ++f) {
            float m = 0;
            for (uint32_t c = 0; c < ch; ++c)
              m += s[f * ch + c];
            mono[f] = m / ch;
          }
          rbAna_.write(mono.data(), frames);
        },
        running_);
  });

  // 渲染线程：播放缓冲→默认设备，按 muted 自动掐声。
  renThread_ = std::thread([this] {
    RenderPlayback ren;
    if (!ren.initialize()) {
      error_ = "渲染初始化失败: " + ren.error();
      running_.store(false);
      return;
    }
    ren.run(rbPlay_, running_, &muted_);
  });

  // 分析线程：滑动窗→声纹→比对目标→设 muted（M3.1：滑动窗+跳步）。
  anaThread_ = std::thread([this] {
    std::vector<float> window;
    std::vector<float> chunk(4096);
    size_t sinceEval = 0; // 距上次判定累计了多少新样本
    while (running_.load()) {
      const uint32_t r = sampleRate_.load();
      const size_t windowLen = (size_t)(r * cfg_.windowSec);
      const size_t hopLen = (size_t)(r * cfg_.hopSec);
      size_t got = rbAna_.read(chunk.data(), chunk.size());
      if (got == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      }
      window.insert(window.end(), chunk.begin(), chunk.begin() + got);
      sinceEval += got;
      // 维持滑动窗：只保留最近 windowLen 个样本（旧的从头部丢掉）
      if (window.size() > windowLen)
        window.erase(window.begin(), window.end() - windowLen);
      // 窗满 且 又滑过一个 hop，就重判一次
      if (window.size() >= windowLen && sinceEval >= hopLen) {
        sinceEval = 0;
        std::vector<float> e;
        std::string er;
        if (emb_.embed(window, r, e, er)) {
          float sim = cosineSimilarity(e, target_);
          lastSim_.store(sim);
          muted_.store(sim > cfg_.threshold); // 是目标 → 掐声
        }
      }
    }
  });

  return true;
}

void AutoMuteEngine::stop() {
  running_.store(false);
  if (capThread_.joinable())
    capThread_.join();
  if (renThread_.joinable())
    renThread_.join();
  if (anaThread_.joinable())
    anaThread_.join();
}
