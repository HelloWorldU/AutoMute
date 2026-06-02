//
// P2 spike：进程 loopback 探针。给一个 PID，抓它的音频，打印电平。
// 目的：验证进程 loopback API 在 MinGW 下能激活、能抓到指定进程的声音。
//   用法: automute_ploop_probe <PID>
//
#include <windows.h>

#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdio>

#include "automute/audio/process_loopback.h"

static std::atomic<bool> g_run{true};
static void onSig(int) { g_run.store(false); }

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);
  if (argc < 2) {
    printf("用法: automute_ploop_probe <PID>\n");
    return 1;
  }
  uint32_t pid = (uint32_t)atoi(argv[1]);
  std::signal(SIGINT, onSig);

  ProcessLoopbackCapture cap;
  if (!cap.initialize(pid)) {
    printf("初始化失败: %s\n", cap.error().c_str());
    return 1;
  }
  printf("✅ 进程 loopback 已激活: PID=%u, %u Hz %u 声道。\n", pid,
         cap.sampleRate(), cap.channels());
  printf("让该进程播放声音，看电平跳动。Ctrl+C 退出。\n");
  fflush(stdout);

  const uint32_t upd = cap.sampleRate() / 10;
  double ss = 0;
  uint32_t acc = 0;
  cap.run(
      [&](const float* s, uint32_t frames, uint32_t ch) {
        for (uint32_t f = 0; f < frames; ++f) {
          float m = 0;
          for (uint32_t c = 0; c < ch; ++c)
            m += s[f * ch + c];
          m /= ch;
          ss += (double)m * m;
        }
        acc += frames;
        if (acc >= upd) {
          float rms = (float)std::sqrt(ss / acc);
          float db = rms > 1e-7f ? 20 * std::log10(rms) : -120.0f;
          printf("\r电平 %6.1f dBFS    ", db);
          fflush(stdout);
          ss = 0;
          acc = 0;
        }
      },
      g_run);
  printf("\n停止。\n");
  return 0;
}
