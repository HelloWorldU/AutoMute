//
// AutoMute — M1 第 1 步：验证“能听见系统声音”。
// 抓 WASAPI loopback，实时打印音量条(dBFS)。播放任意声音即可看到电平跳动。
// Ctrl+C 退出。
//
#include "automute/audio/loopback_capture.h"

#include <windows.h>

#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <string>

static std::atomic<bool> g_running{true};

static void onSignal(int) { g_running.store(false); }

// 把 RMS 振幅画成一个简单的电平条。
static void printMeter(float rms) {
  float db = (rms > 1e-7f) ? 20.0f * std::log10(rms) : -120.0f;
  // 把 [-60, 0] dB 映射到 0..40 格
  int filled = static_cast<int>((db + 60.0f) / 60.0f * 40.0f);
  if (filled < 0)
    filled = 0;
  if (filled > 40)
    filled = 40;

  char bar[41];
  for (int i = 0; i < 40; ++i)
    bar[i] = (i < filled) ? '#' : '-';
  bar[40] = '\0';
  printf("\r[%s] %6.1f dBFS  ", bar, db);
  fflush(stdout);
}

int main() {
  SetConsoleOutputCP(CP_UTF8); // 让中文输出不乱码
  std::signal(SIGINT, onSignal);

  LoopbackCapture cap;
  if (!cap.initialize()) {
    fprintf(stderr, "初始化失败: %s\n", cap.error().c_str());
    return 1;
  }

  printf("正在监听系统输出: %u Hz, %u 声道。播放点声音试试，Ctrl+C 退出。\n",
         cap.sampleRate(), cap.channels());

  // 每累计约 50ms 更新一次电平，避免刷屏太快。
  const uint32_t updateEvery = cap.sampleRate() / 20;
  double sumSquares = 0.0;
  uint32_t accFrames = 0;

  cap.run(
      [&](const float* samples, uint32_t frames, uint32_t channels) {
        for (uint32_t f = 0; f < frames; ++f) {
          // 多声道取平均，得到单声道能量。
          float mono = 0.0f;
          for (uint32_t c = 0; c < channels; ++c)
            mono += samples[f * channels + c];
          mono /= static_cast<float>(channels);
          sumSquares += static_cast<double>(mono) * mono;
        }
        accFrames += frames;

        if (accFrames >= updateEvery) {
          float rms = static_cast<float>(std::sqrt(sumSquares / accFrames));
          printMeter(rms);
          sumSquares = 0.0;
          accFrames = 0;
        }
      },
      g_running);

  if (!cap.error().empty()) {
    fprintf(stderr, "\n抓取出错: %s\n", cap.error().c_str());
    return 1;
  }
  printf("\n已停止。\n");
  return 0;
}
