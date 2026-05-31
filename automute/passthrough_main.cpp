//
// AutoMute — M1 第 2 步：回放闭环。
//
//   抓取线程(生产者) ──► 无锁环形缓冲 ──► 渲染线程(消费者) ──► 默认输出设备
//                              │
//                       静音开关在抓取端：muted 时写静音而非真音频
//
// 键盘： m = 切换静音   q = 退出
// ⚠️ loopback→同一设备会回声反馈，仅作短测试（建议把系统音量调低）。
//
#include "automute/audio/loopback_capture.h"
#include "automute/audio/render_playback.h"
#include "automute/audio/ring_buffer.h"

#include <windows.h>

#include <atomic>
#include <conio.h> // _kbhit / _getch（Windows 非阻塞读键）
#include <cstdio>
#include <thread>
#include <vector>

int main() {
  SetConsoleOutputCP(CP_UTF8);

  // 环形缓冲：给 1 秒余量（最多 48kHz×8 声道），抓取/渲染速率波动时当蓄水池。
  SpscRingBuffer<float> ring(48000 * 8);

  std::atomic<bool> running{true};
  std::atomic<bool> muted{false};
  std::atomic<uint32_t> sampleRate{48000};
  std::atomic<uint32_t> channels{2};

  // ---- 抓取线程（生产者）----
  std::thread captureThread([&] {
    LoopbackCapture cap;
    if (!cap.initialize()) {
      fprintf(stderr, "抓取初始化失败: %s\n", cap.error().c_str());
      running.store(false);
      return;
    }
    sampleRate.store(cap.sampleRate());
    channels.store(cap.channels());

    std::vector<float> silence;
    cap.run(
        [&](const float* samples, uint32_t frames, uint32_t ch) {
          const size_t n = static_cast<size_t>(frames) * ch;
          if (muted.load(std::memory_order_relaxed)) {
            // 静音：往缓冲写 0，保持时间轴连续（而不是跳过）。
            silence.assign(n, 0.0f);
            ring.write(silence.data(), n);
          } else {
            ring.write(samples, n);
          }
        },
        running);
  });

  // ---- 渲染线程（消费者）----
  std::thread renderThread([&] {
    RenderPlayback ren;
    if (!ren.initialize()) {
      fprintf(stderr, "渲染初始化失败: %s\n", ren.error().c_str());
      running.store(false);
      return;
    }
    ren.run(ring, running);
  });

  printf("回放闭环已启动。m=切换静音  q=退出\n");
  printf("⚠️ 同设备 loopback 有回声，建议调低系统音量短测。\n\n");

  // ---- 主线程：键盘 + 延迟显示 ----
  while (running.load()) {
    if (_kbhit()) {
      int c = _getch();
      if (c == 'q' || c == 'Q') {
        running.store(false);
        break;
      }
      if (c == 'm' || c == 'M') {
        bool now = !muted.load();
        muted.store(now);
        printf("\n[%s]\n", now ? "静音 MUTED" : "放行 PASS");
      }
    }

    // 管线缓冲延迟 ≈ 环形缓冲里堆积的音频时长。
    const uint32_t sr = sampleRate.load();
    const uint32_t ch = channels.load();
    const double bufferedMs =
        1000.0 * (ring.size() / static_cast<double>(ch)) / sr;
    printf("\r管线缓冲: %6.1f ms   [%s]   ", bufferedMs,
           muted.load() ? "MUTED" : "PASS ");
    fflush(stdout);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  captureThread.join();
  renderThread.join();
  printf("\n已停止。\n");
  return 0;
}
