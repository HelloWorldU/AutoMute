//
// AutoMute —— 主程序（M2.5：自动定向静音闭环）。
//
//   抓取线程 ─┬─► 播放 ring buffer ──► 渲染线程 ──► 输出设备
//             │                          ▲ 按 muted 掐声
//             └─► 分析 ring buffer ──► 分析线程：声纹判定 → 设 muted
//
//   检测到“目标在说话”就自动静音其余照常。q 退出。
//   用法: automute_app [目标.wav] [model.onnx]
//
//   ⚠️ 同设备 loopback 抓取+回放会有回声反馈，仅作短测试（建议调低系统音量）。
//      真正落地需把直播 App 输出路由到虚拟声卡（见 docs/DESIGN.md）。
//
#include <windows.h>

#include <atomic>
#include <chrono>
#include <conio.h>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "automute/audio/embedder.h"
#include "automute/audio/loopback_capture.h"
#include "automute/audio/render_playback.h"
#include "automute/audio/ring_buffer.h"

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);
  std::string targetWav =
      (argc > 1) ? argv[1] : "models/test_speakers/spkA_1272_1.wav";
  std::string model =
      (argc > 2) ? argv[2] : "models/voxceleb_ECAPA512_LM.onnx";
  const float THRESH = 0.5f;

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
  fflush(stdout);

  SpscRingBuffer<float> rbPlay(48000 * 8); // 播放：交错立体声
  SpscRingBuffer<float> rbAna(48000 * 4);  // 分析：单声道
  std::atomic<bool> running{true};
  std::atomic<bool> muted{false};
  std::atomic<uint32_t> sampleRate{48000};
  std::atomic<float> lastSim{0.0f};

  // 抓取线程：交错→播放缓冲，降混单声道→分析缓冲
  std::thread capThread([&] {
    LoopbackCapture cap;
    if (!cap.initialize()) {
      printf("抓取初始化失败: %s\n", cap.error().c_str());
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

  // 渲染线程：播放缓冲→设备，按 muted 自动掐声
  std::thread renThread([&] {
    RenderPlayback ren;
    if (!ren.initialize()) {
      printf("渲染初始化失败: %s\n", ren.error().c_str());
      running.store(false);
      return;
    }
    ren.run(rbPlay, running, &muted);
  });

  // 分析线程：攒窗→声纹→比对目标→设 muted
  std::thread anaThread([&] {
    std::vector<float> window;
    std::vector<float> chunk(4096);
    while (running.load()) {
      const uint32_t r = sampleRate.load();
      const size_t windowLen = (size_t)(r * 1.5);
      size_t got = rbAna.read(chunk.data(), chunk.size());
      if (got > 0)
        window.insert(window.end(), chunk.begin(), chunk.begin() + got);
      if (window.size() >= windowLen) {
        std::vector<float> e;
        std::string er;
        if (emb.embed(window, r, e, er)) {
          float sim = cosineSimilarity(e, target);
          lastSim.store(sim);
          muted.store(sim > THRESH); // 是目标 → 掐声
        }
        window.clear();
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }
  });

  printf("AutoMute 运行中：检测到目标说话即自动静音，其余照常。q 退出。\n");
  printf("⚠️ 同设备 loopback 有回声，建议调低系统音量短测。\n\n");
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
