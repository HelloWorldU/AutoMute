#pragma once
//
// 音频会话枚举（产品化 P2.2/P2.3）：
//   - 列出"哪些进程在出声"（PID + 进程名 + 是否活动）→ 供用户选目标 App。
//   - 把某进程的直接输出静音 → 让用户只听 AutoMute 处理后的版本。
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

// 把某进程的会话静音/取消静音（直接输出）。返回是否找到并设置成功。
bool setProcessMuted(uint32_t pid, bool muted);
