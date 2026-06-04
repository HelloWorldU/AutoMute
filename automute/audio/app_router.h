#pragma once
//
// 每应用音频路由（M4.3）：把目标 App 的输出改路由到 VB-CABLE，退出还原。
// 与 AutoMuteEngine 解耦——引擎只管抓取/门控/渲染，路由是正交关注点，
// 由前端（app_main / 未来 GUI）驱动本模块。
//
// 分两层：
//   ① 端点枚举 / VB-CABLE 检测（M4.3a，本文件）——纯文档化 COM，稳。
//   ② 未公开 IAudioPolicyConfig 的 route/restore（M4.3b）——版本脆，另起 AppRouter 类，
//      失败必有"引导手动"兜底（见 m4-app-shell.md 的 M4.3 对齐）。
//
#include <cstdint>
#include <string>
#include <vector>

namespace approuter {

// 一个渲染端点：id 是 COM 设备标识字符串（路由 API 要它），name 是给人看的友好名。
struct Endpoint {
  std::string id;
  std::string name;
};

// 枚举所有【活动】渲染端点（id + 友好名）。失败/无设备返回空。
std::vector<Endpoint> listRenderEndpoints();

// 找友好名【包含】nameSubstr 的第一个渲染端点；命中填 out 返回 true。
bool findRenderEndpoint(const std::string& nameSubstr, Endpoint& out);

// 便捷：VB-CABLE 输入端（"CABLE Input"）是否存在；存在则填 endpointId 返回 true。
// 这是"路由隔离原声"的目标设备，也是 M4.3 自动路由的前提。
bool cableInstalled(std::string& endpointId);

// ── M4.3b：每应用默认输出路由（未公开 IAudioPolicyConfig） ──
// 把目标 App 的输出改路由到指定端点（如 VB-CABLE），退出还原。
// 用未公开的 WinRT 接口 IAudioPolicyConfigFactory（Win 设置「应用音量和设备首选项」
// 背后那套）。接口版本脆 → available() 为 false 时前端必须走"引导手动"兜底。
//
// 持久性陷阱：路由是 persisted（写进系统、关机重启都在），不还原则目标 App
// 永久被改到 CABLE。正常退出靠 RAII 析构还原；崩溃/强杀靠 journal 启动兜底
// （见 m4-app-shell.md 的 M4.3 对齐与 C++ 残留说明）。
class AppRouter {
public:
  AppRouter();
  ~AppRouter(); // RAII：析构自动 restore()
  AppRouter(const AppRouter&) = delete;
  AppRouter& operator=(const AppRouter&) = delete;

  // 未公开接口是否激活成功。false → 前端走"引导手动设置"兜底。
  bool available() const;

  // 把进程 pid 的默认渲染端点路由到 endpointId（approuter::Endpoint::id）。
  // 先捕获原值并写 journal（崩溃兜底），再设 eMultimedia/eConsole 两 role。
  // 失败返回 false 填 err。pid 须有音频会话，否则系统返回 E_INVALIDARG。
  bool route(uint32_t pid, const std::string& endpointId, std::string& err);

  // 还原 route() 改过的端点（幂等，可重复；析构自动调）。
  void restore();
  bool routed() const;

  // 逃生舱：清除【所有】应用的持久路由。崩溃残留的最后兜底（会清掉别的 App
  // 的有意路由，慎用，作为 UI 上的手动按钮）。
  bool clearAllRoutes(std::string& err);

  // 启动时调一次：若上次非正常退出留下 journal，按它还原并删 journal。
  static void recoverStaleRoutes();

private:
  struct Impl;
  Impl* impl_;
};

} // namespace approuter
