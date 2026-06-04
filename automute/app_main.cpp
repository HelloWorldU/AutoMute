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
//   运行中热键（M4.2 多目标 + 在线抓取冰测）:
//     c        抓取最近 3s 当前说话人 → 输入名字 → 登记为新目标（静音开关默认关）
//     1..9     切换第 N 个目标的静音开关（开了才会掐这个人）
//     q        退出
//
//   准备(一次性): 装 VB-CABLE。M4.3c 起【程序自动路由】目标 App 到 CABLE、退出还原，
//                 不用再手动去「音量合成器」设；自动路由不可用时回退引导手动。
//
#include <windows.h>

#include <chrono>
#include <conio.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

#include "automute/audio/app_router.h"
#include "automute/audio/audio_sessions.h"
#include "automute/engine.h"

// Ctrl+C / 关闭窗口时也把路由还原（避免目标 App 被永久改到 CABLE）。
// 强杀/崩溃这条来不及跑，靠下次启动 recoverStaleRoutes() 据 journal 兜底。
static approuter::AppRouter* g_router = nullptr;
static BOOL WINAPI consoleHandler(DWORD) {
  if (g_router)
    g_router->restore();
  return FALSE; // 交还默认处理，进程照常终止
}

int main(int argc, char** argv) {
  SetConsoleOutputCP(CP_UTF8);
  // 启动先清理上次非正常退出可能残留的路由（崩溃/强杀兜底）。
  approuter::AppRouter::recoverStaleRoutes();
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

  // ── M4.3c：自动把目标 App 路由到 VB-CABLE（退出/Ctrl+C 自动还原）──
  // 失败逐级回退到「引导手动」，整条链任何一环不成都不影响程序继续跑。
  approuter::AppRouter router;
  g_router = &router;
  SetConsoleCtrlHandler(consoleHandler, TRUE);
  if (!router.available()) {
    printf("⚠️ 自动路由不可用（本 Windows 版本不支持隐藏接口）。\n"
           "   请手动在「音量合成器/应用音量」把目标 App 输出设为 CABLE Input。\n");
  } else {
    std::string cableId;
    if (!approuter::cableInstalled(cableId)) {
      printf("⚠️ 未检测到 VB-CABLE，无法自动路由。\n"
             "   装一下：https://vb-audio.com/Cable/ ，或先手动路由。\n");
    } else if (router.route(procPid, cableId, err)) {
      printf("✅ 已自动把目标 App 路由到 VB-CABLE，退出时自动还原。\n");
    } else {
      printf("⚠️ 自动路由失败：%s\n"
             "   请手动在「音量合成器」把目标 App 输出设为 CABLE Input。\n",
             err.c_str());
    }
  }

  if (!engine.start(procPid, err)) {
    printf("%s\n", err.c_str());
    return 1;
  }

  printf("AutoMute 运行中：检测到【开了静音开关】的目标说话即掐声，其余照常。\n");
  printf("热键：c 抓取当前说话人登记新目标 / 1-9 切第 N 个目标静音开关 / q 退出。\n\n");
  while (engine.running()) {
    if (_kbhit()) {
      int k = _getch();
      if (k == 'q')
        break;
      if (k == 'c') {
        // 在线圈人：抓最近 3s → 命名 → 登记（静音开关默认关，圈完先看仪表）。
        std::vector<float> snap;
        uint32_t sr = 0;
        if (engine.snapshotRecent(3.0f, snap, sr) && !snap.empty()) {
          printf("\n抓到 %.2fs 音频，输入名字回车登记（直接回车自动命名）: ",
                 snap.size() / (float)sr);
          fflush(stdout);
          std::string name;
          std::getline(std::cin, name);
          if (name.empty())
            name = "目标" + std::to_string(engine.targetCount() + 1);
          std::string er;
          int idx = engine.addTarget(name, snap, sr, er);
          if (idx < 0)
            printf("登记失败: %s\n", er.c_str());
          else
            printf("已登记 [%d] %s —— 按数字键 %d 开它的静音开关。\n", idx,
                   name.c_str(), idx + 1);
        } else {
          printf("\n快照为空（还没抓到声音，等目标 App 出声后再试）。\n");
        }
      } else if (k >= '1' && k <= '9') {
        size_t idx = (size_t)(k - '1');
        if (idx < engine.targetCount()) {
          AutoMuteEngine::TargetView v = engine.targetAt(idx);
          engine.setTargetMuted(idx, !v.muted);
        }
      }
    }
    // 状态行：聚合 + 每个目标的相似度/开关。
    printf("\r%s 聚合%.3f | ", engine.muted() ? "🔇" : "🔊",
           engine.similarity());
    size_t n = engine.targetCount();
    for (size_t i = 0; i < n; ++i) {
      AutoMuteEngine::TargetView v = engine.targetAt(i);
      printf("[%zu]%s%.2f%s ", i + 1, v.name.c_str(), v.similarity,
             v.muted ? "🔒" : "🔓");
    }
    printf("        ");
    fflush(stdout);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  engine.stop();
  router.restore(); // 还原目标 App 的输出设备（幂等；析构也会兜一次）
  g_router = nullptr;
  if (!engine.error().empty())
    printf("\n%s\n", engine.error().c_str());
  printf("\n已停止，已还原设备路由。\n");
  return 0;
}
