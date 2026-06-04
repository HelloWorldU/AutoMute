//
// AutoMute —— 命令行前端。核心逻辑在 AutoMuteEngine，这里只管参数/输出/键盘。
//
//   目标 App(PID) ─► 引擎(进程loopback抓取→声纹门控→渲染) ─► 喇叭(默认设备)
//
//   抓取与“用户耳朵”必须分路：把目标 App 的输出路由到虚拟声卡(用户听不到)，
//   引擎从进程 loopback 抓它(不静音→抓得到)、做声纹门控、再渲染到喇叭。
//   ⚠️ 不能用 SetMute 静音目标 App：会话静音在 loopback 抽头的【上游】，
//      一静音连我们自己也抓不到（已真机验证：mute→loopback 变 -120dB）。
//
//   用法:
//     automute_app --apps                              列出在出声的进程及 PID
//     automute_app --proc <PID> [目标.wav] [model.onnx]
//                  [--window 0.75] [--hop 0.25] [--thresh 0.5]   ← 扫参,免重编译
//
//   准备(一次性): 装 VB-CABLE，在 Windows「音量合成器/应用音量」把目标 App 的输出
//                 设为 "CABLE Input"，系统默认输出保持喇叭。
//
#include <windows.h>

#include <chrono>
#include <conio.h>
#include <cstdio>
#include <string>
#include <thread>

#include "automute/audio/audio_sessions.h"
#include "automute/engine.h"

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);
  AutoMuteEngine::Config cfg;
  uint32_t procPid = 0; // 目标 App 的 PID（必填，--proc 指定）

  // 参数解析：--apps 列进程；--proc 选目标 App；以 .onnx 结尾当模型，其余当目标 wav。
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--apps") {
      printAudioSessions();
      return 0;
    } else if (a == "--proc" && i + 1 < argc) {
      procPid = (uint32_t)atoi(argv[++i]);
    } else if (a == "--window" && i + 1 < argc) {
      cfg.windowSec = (float)atof(argv[++i]);
    } else if (a == "--hop" && i + 1 < argc) {
      cfg.hopSec = (float)atof(argv[++i]);
    } else if (a == "--thresh" && i + 1 < argc) {
      cfg.threshold = (float)atof(argv[++i]);
    } else if (a.size() > 5 && a.substr(a.size() - 5) == ".onnx") {
      cfg.model = a;
    } else {
      cfg.targetWav = a;
    }
  }

  if (procPid == 0) {
    printf("请用 --proc <PID> 指定目标 App。\n");
    printf("先跑 `automute_app --apps` 看哪些进程在出声及其 PID。\n");
    return 1;
  }

  AutoMuteEngine engine;
  std::string err;
  if (!engine.prepare(cfg, err)) {
    printf("%s\n", err.c_str());
    return 1;
  }
  printf("已登记目标声纹: %s\n", cfg.targetWav.c_str());
  printf("调参：窗 %.2fs / 跳步 %.2fs / 阈值 %.2f\n", cfg.windowSec, cfg.hopSec,
         cfg.threshold);
  printf("提示：确保目标 App 已路由到虚拟声卡，否则会听到原声叠加。\n");

  if (!engine.start(procPid, err)) {
    printf("%s\n", err.c_str());
    return 1;
  }

  printf("AutoMute 运行中：检测到目标说话即自动静音，其余照常。q 退出。\n\n");
  while (engine.running()) {
    if (_kbhit() && (_getch() == 'q'))
      break;
    printf("\r相似度 %.3f   %s          ", engine.similarity(),
           engine.muted() ? "🔇 静音中" : "🔊 放行");
    fflush(stdout);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  engine.stop();
  if (!engine.error().empty())
    printf("\n%s\n", engine.error().c_str());
  printf("\n已停止。\n");
  return 0;
}
