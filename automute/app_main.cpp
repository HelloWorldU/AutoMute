//
// AutoMute —— 主程序（P2.4：进程级定向静音）。
//
//   目标 App(PID) ─► 进程 loopback 抓它的音频 ─┬─► 播放 ring buffer ─► 渲染线程 ─► 喇叭(默认设备)
//                                             │                       ▲ 按 muted 掐声
//                                             └─► 分析 ring buffer ─► 分析线程：声纹判定 → 设 muted
//
//   抓取与“用户耳朵”必须分路：把目标 App 的输出路由到虚拟声卡(用户听不到)，
//   我们从进程 loopback 抓它(不静音→抓得到)、做声纹门控、再渲染到喇叭。
//   ⚠️ 不能用 SetMute 静音目标 App：会话静音在 loopback 抽头的【上游】，
//      一静音连我们自己也抓不到（已真机验证：mute→loopback 变 -120dB）。
//   检测到“目标在说话”就自动静音，其余照常。q 退出。
//
//   用法:
//     automute_app --apps                              列出在出声的进程及 PID
//     automute_app --proc <PID> [目标.wav] [model.onnx]
//
//   准备(一次性): 装 VB-CABLE，在 Windows「音量合成器/应用音量」把目标 App 的输出
//                 设为 "CABLE Input"，系统默认输出保持喇叭。
//
#include <windows.h>

#include <atomic>
#include <chrono>
#include <conio.h>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "automute/audio/audio_sessions.h"
#include "automute/audio/embedder.h"
#include "automute/audio/process_loopback.h"
#include "automute/audio/render_playback.h"
#include "automute/audio/ring_buffer.h"

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);
  std::string targetWav = "models/test_speakers/spkA_1272_1.wav";
  std::string model = "models/voxceleb_ECAPA512_LM.onnx";
  uint32_t procPid = 0; // 目标 App 的 PID（必填，--proc 指定）
  // ── M3 调参旋钮 ──
  const float THRESH = 0.5f;     // 相似度 > 阈值 判为目标
  const float WINDOW_SEC = 1.5f; // 声纹判定窗长（越短越跟手、但越短越不准，有物理地板）
  const float HOP_SEC = 0.25f;   // 每滑过这么久重判一次（滑动窗，不再清空重攒）

  // 参数解析：--apps 列进程；--proc 选目标 App；以 .onnx 结尾当模型，其余当目标 wav。
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--apps") {
      printAudioSessions();
      return 0;
    } else if (a == "--proc" && i + 1 < argc) {
      procPid = (uint32_t)atoi(argv[++i]);
    } else if (a.size() > 5 && a.substr(a.size() - 5) == ".onnx") {
      model = a;
    } else {
      targetWav = a;
    }
  }

  if (procPid == 0) {
    printf("请用 --proc <PID> 指定目标 App。\n");
    printf("先跑 `automute_app --apps` 看哪些进程在出声及其 PID。\n");
    return 1;
  }

  // 登记目标声纹
  Embedder emb;
  std::string err;
  if (!emb.load(model, err)) {
    printf("%s\n", err.c_str());
    return 1;
  }
  std::vector<float> target;
  if (!emb.embedWav(targetWav, target, err)) {
    printf("enroll 目标失败: %s\n", err.c_str());
    return 1;
  }
  printf("已登记目标声纹: %s\n", targetWav.c_str());
  printf("提示：确保目标 App 已路由到虚拟声卡，否则会听到原声叠加。\n");
  fflush(stdout);

  SpscRingBuffer<float> rbPlay(48000 * 8); // 播放：交错立体声
  SpscRingBuffer<float> rbAna(48000 * 4);  // 分析：单声道
  std::atomic<bool> running{true};
  std::atomic<bool> muted{false};
  std::atomic<uint32_t> sampleRate{48000};
  std::atomic<float> lastSim{0.0f};

  // 抓取线程：进程 loopback → 交错写播放缓冲，降混单声道写分析缓冲
  std::thread capThread([&] {
    ProcessLoopbackCapture cap;
    if (!cap.initialize(procPid)) {
      printf("\n进程抓取初始化失败: %s\n", cap.error().c_str());
      running.store(false);
      return;
    }
    sampleRate.store(cap.sampleRate());
    std::vector<float> mono;
    cap.run(
        [&](const float* s, uint32_t frames, uint32_t ch) {
          rbPlay.write(s, (size_t)frames * ch);
          mono.resize(frames);
          for (uint32_t f = 0; f < frames; ++f) {
            float m = 0;
            for (uint32_t c = 0; c < ch; ++c)
              m += s[f * ch + c];
            mono[f] = m / ch;
          }
          rbAna.write(mono.data(), frames);
        },
        running);
  });

  // 渲染线程：播放缓冲→默认设备，按 muted 自动掐声
  std::thread renThread([&] {
    RenderPlayback ren;
    if (!ren.initialize()) {
      printf("\n渲染初始化失败: %s\n", ren.error().c_str());
      running.store(false);
      return;
    }
    ren.run(rbPlay, running, &muted);
  });

  // 分析线程：滑动窗→声纹→比对目标→设 muted（M3.1：滑动窗+跳步，不清空重攒）
  std::thread anaThread([&] {
    std::vector<float> window;
    std::vector<float> chunk(4096);
    size_t sinceEval = 0; // 距上次判定累计了多少新样本
    while (running.load()) {
      const uint32_t r = sampleRate.load();
      const size_t windowLen = (size_t)(r * WINDOW_SEC);
      const size_t hopLen = (size_t)(r * HOP_SEC);
      size_t got = rbAna.read(chunk.data(), chunk.size());
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
        if (emb.embed(window, r, e, er)) {
          float sim = cosineSimilarity(e, target);
          lastSim.store(sim);
          muted.store(sim > THRESH); // 是目标 → 掐声
        }
      }
    }
  });

  printf("AutoMute 运行中：检测到目标说话即自动静音，其余照常。q 退出。\n\n");
  while (running.load()) {
    if (_kbhit() && (_getch() == 'q')) {
      running.store(false);
      break;
    }
    printf("\r相似度 %.3f   %s          ", lastSim.load(),
           muted.load() ? "🔇 静音中" : "🔊 放行");
    fflush(stdout);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  capThread.join();
  renThread.join();
  anaThread.join();
  printf("\n已停止。\n");
  return 0;
}
