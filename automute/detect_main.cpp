//
// M2.4b 实时声纹检测：
//   抓取线程 → 分析 ring buffer → 分析线程攒窗(~1.5s) → Embedder 算声纹
//   → 与"目标声纹"算余弦 → 超阈值判定"是目标在说话"，实时打印。
//
//   用法: automute_detect [目标.wav] [model.onnx]
//   先 enroll 一段目标的声音，然后播放任意音频，看检测结果。
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
#include "automute/audio/ring_buffer.h"

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);
  std::string targetWav =
      (argc > 1) ? argv[1] : "models/test_speakers/spkA_1272_1.wav";
  std::string model =
      (argc > 2) ? argv[2] : "models/voxceleb_ECAPA512_LM.onnx";
  const float THRESH = 0.5f;

  // 1) 加载模型 + 登记(enroll)目标声纹
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
  printf("已登记目标声纹: %s (%zu 维)\n", targetWav.c_str(), target.size());
  fflush(stdout);

  SpscRingBuffer<float> rb(48000 * 4); // 分析缓冲，约 4 秒余量
  std::atomic<bool> running{true};
  std::atomic<uint32_t> sampleRate{48000};

  // 2) 抓取线程：把系统声音降成单声道写进分析缓冲
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
          mono.resize(frames);
          for (uint32_t f = 0; f < frames; ++f) {
            float m = 0;
            for (uint32_t c = 0; c < ch; ++c)
              m += s[f * ch + c];
            mono[f] = m / ch;
          }
          rb.write(mono.data(), frames);
        },
        running);
  });

  // 3) 分析线程：攒满一窗就算声纹、比对目标、打印判定
  std::thread anaThread([&] {
    std::vector<float> window;
    std::vector<float> chunk(4096);
    while (running.load()) {
      const uint32_t r = sampleRate.load();
      const size_t windowLen = (size_t)(r * 1.5); // 1.5 秒一窗

      size_t got = rb.read(chunk.data(), chunk.size());
      if (got > 0)
        window.insert(window.end(), chunk.begin(), chunk.begin() + got);

      if (window.size() >= windowLen) {
        std::vector<float> e;
        std::string er;
        if (emb.embed(window, r, e, er)) {
          float sim = cosineSimilarity(e, target);
          printf("声纹相似度 %.3f  → %s\n", sim,
                 sim > THRESH ? "【是目标在说话】" : "不是目标");
          fflush(stdout);
        }
        window.clear();
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }
  });

  printf("实时检测中… 播放声音试试，q 退出。\n");
  while (running.load()) {
    if (_kbhit() && (_getch() == 'q')) {
      running.store(false);
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  capThread.join();
  anaThread.join();
  printf("已停止。\n");
  return 0;
}
