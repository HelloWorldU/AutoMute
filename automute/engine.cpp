#include "automute/engine.h"

#include <algorithm>
#include <chrono>

#include "automute/audio/process_loopback.h"
#include "automute/audio/render_playback.h"

AutoMuteEngine::~AutoMuteEngine() { stop(); }

bool AutoMuteEngine::prepare(const Config& cfg, std::string& err) {
  cfg_ = cfg;
  if (!emb_.load(cfg_.model, err))
    return false;
  // CLI 旧行为：给了目标 wav 就登记为首个目标并开静音开关（启动即生效）。
  // GUI 走在线抓取可留空，启动后再 addTarget。
  if (!cfg_.targetWav.empty()) {
    int idx = addTargetFromWav("目标", cfg_.targetWav, err);
    if (idx < 0)
      return false;
    setTargetMuted((size_t)idx, true);
  }
  prepared_ = true;
  return true;
}

int AutoMuteEngine::enroll(const std::string& name, std::vector<float>&& emb) {
  std::lock_guard<std::mutex> lk(targetsMu_);
  auto t = std::make_unique<Target>();
  t->name = name;
  t->emb = std::move(emb);
  targets_.push_back(std::move(t));
  return (int)targets_.size() - 1;
}

int AutoMuteEngine::addTarget(const std::string& name,
                              const std::vector<float>& mono, uint32_t sr,
                              std::string& err) {
  std::vector<float> e;
  if (!emb_.embed(mono, sr, e, err)) {
    err = "登记 '" + name + "' 失败: " + err;
    return -1;
  }
  return enroll(name, std::move(e));
}

int AutoMuteEngine::addTargetFromWav(const std::string& name,
                                     const std::string& wavPath,
                                     std::string& err) {
  std::vector<float> e;
  if (!emb_.embedWav(wavPath, e, err)) {
    err = "登记 '" + name + "' 失败: " + err;
    return -1;
  }
  return enroll(name, std::move(e));
}

void AutoMuteEngine::setTargetMuted(size_t idx, bool on) {
  std::lock_guard<std::mutex> lk(targetsMu_);
  if (idx < targets_.size())
    targets_[idx]->muted.store(on);
}

void AutoMuteEngine::renameTarget(size_t idx, const std::string& name) {
  std::lock_guard<std::mutex> lk(targetsMu_);
  if (idx < targets_.size())
    targets_[idx]->name = name;
}

void AutoMuteEngine::removeTarget(size_t idx) {
  std::lock_guard<std::mutex> lk(targetsMu_);
  if (idx < targets_.size())
    targets_.erase(targets_.begin() + idx);
}

size_t AutoMuteEngine::targetCount() const {
  std::lock_guard<std::mutex> lk(targetsMu_);
  return targets_.size();
}

AutoMuteEngine::TargetView AutoMuteEngine::targetAt(size_t idx) const {
  std::lock_guard<std::mutex> lk(targetsMu_);
  TargetView v{};
  if (idx < targets_.size()) {
    v.name = targets_[idx]->name;
    v.similarity = targets_[idx]->lastSim.load();
    v.muted = targets_[idx]->muted.load();
  }
  return v;
}

void AutoMuteEngine::pushHistory(const float* mono, size_t n) {
  std::lock_guard<std::mutex> lk(histMu_);
  if (hist_.empty()) {
    const size_t cap = (size_t)(kHistorySec * sampleRate_.load());
    hist_.assign(cap ? cap : 1, 0.0f);
    histWrite_ = 0;
    histFilled_ = 0;
  }
  const size_t cap = hist_.size();
  // 一次写入超过容量时只保留尾部 cap 个（更早的本来也会被覆盖掉）。
  if (n >= cap) {
    mono += (n - cap);
    n = cap;
  }
  for (size_t i = 0; i < n; ++i) {
    hist_[histWrite_] = mono[i];
    histWrite_ = (histWrite_ + 1) % cap;
  }
  histFilled_ = std::min(histFilled_ + n, cap);
}

bool AutoMuteEngine::snapshotRecent(float seconds, std::vector<float>& outMono,
                                    uint32_t& sr) const {
  std::lock_guard<std::mutex> lk(histMu_);
  sr = sampleRate_.load();
  if (histFilled_ == 0) {
    outMono.clear();
    return false;
  }
  const size_t cap = hist_.size();
  size_t want = (size_t)(seconds * sr);
  if (want > histFilled_)
    want = histFilled_;
  outMono.resize(want);
  // 最近 want 个样本：从 (histWrite_ - want) 处环形读到 histWrite_。
  const size_t start = (histWrite_ + cap - want) % cap;
  for (size_t i = 0; i < want; ++i)
    outMono[i] = hist_[(start + i) % cap];
  return true;
}

bool AutoMuteEngine::start(uint32_t pid, std::string& err) {
  if (running_.load()) {
    err = "引擎已在运行";
    return false;
  }
  if (!prepared_) {
    err = "请先 prepare() 加载模型";
    return false;
  }
  error_.clear();
  running_.store(true);

  // 抓取线程：进程 loopback → 交错写播放缓冲，降混单声道写分析缓冲 + 历史环。
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
          pushHistory(mono.data(), frames);
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

  // 分析线程：滑动窗→声纹→比对【每个】目标→设各自 lastSim 与全局聚合
  // （M3.1 滑动窗+跳步；M4.2 多目标：任一开了静音的目标命中就掐）。
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
        if (!emb_.embed(window, r, e, er)) // 慢（ONNX）：放在锁外
          continue;
        // 持锁比对每个目标（cosine 很便宜），这样改名/删除目标都安全——
        // 删除在锁内 erase，分析线程不会拿到悬空指针。
        float best = 0.0f;
        bool anyMute = false;
        {
          std::lock_guard<std::mutex> lk(targetsMu_);
          for (auto& up : targets_) {
            float sim = cosineSimilarity(e, up->emb);
            up->lastSim.store(sim);
            if (sim > best)
              best = sim;
            if (up->muted.load() && sim > cfg_.threshold)
              anyMute = true; // 开了静音的目标命中 → 掐
          }
        }
        lastSim_.store(best);
        muted_.store(anyMute);
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
