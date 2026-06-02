#pragma once
//
// 音频会话枚举（产品化 P2.2）：
//   列出"哪些进程在出声"（PID + 进程名 + 是否活动）→ 供用户选目标 App。
//
// 注：曾有 setProcessMuted()（P2.3）想静音目标 App 直接输出，但真机验证发现
// 会话静音在进程 loopback 抽头的上游，一静音连我们的抓取也变静音，故已移除。
// 隔离原声靠把目标 App 路由到虚拟声卡（见 app_main / productization.md）。
//
#include <cstdint>
#include <string>
#include <vector>

struct AudioSessionInfo {
  uint32_t pid;
  std::string name; // 进程名（exe）
  bool active;      // 是否正在出声
};

// 列出默认输出设备上的音频会话。
std::vector<AudioSessionInfo> listAudioSessions();
void printAudioSessions();
