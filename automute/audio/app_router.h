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

} // namespace approuter
