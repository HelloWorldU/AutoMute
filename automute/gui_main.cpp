//
// AutoMute GUI 外壳（M4.4）—— C++ webview(WebView2) 宿主，前端 HTML/CSS/JS。
//
//   引擎(抓取→声纹门控→渲染) + 自动路由(目标App→VB-CABLE) 都在 C++ 这边，
//   webview 只画界面。JS↔C++ 用 bind 暴露动作，前端 setInterval 轮询 getStatus。
//
//   绑定（JS 调 window.xxx(...) → Promise）：
//     listApps()            → 在出声的进程列表 [{pid,name,active,device}]
//     start(pid)            → 自动路由 + 启动引擎；{ok,msg}
//     stop()                → 停引擎 + 还原路由
//     capture(name)         → 抓最近 3s 当前说话人登记为目标；{ok,idx,msg}
//     setMuted(idx,on)      → 切某目标静音开关
//     getStatus()           → {running,muted,similarity,routed,targets:[...]}
//     cableStatus()         → {available,installed}
//
#include <windows.h>

#include <commctrl.h> // SetWindowSubclass / DefSubclassProc（无边框窗口）
#include <dwmapi.h>   // DwmExtendFrameIntoClientArea（投影）
#include <uxtheme.h>  // MARGINS
#include <windowsx.h> // GET_X_LPARAM / GET_Y_LPARAM

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "automute/audio/app_router.h"
#include "automute/audio/audio_sessions.h"
#include "automute/engine.h"
#include "webview.h"

using webview::detail::json_escape;
using webview::detail::json_parse;

namespace {

// GUI 持有的全部 C++ 状态。绑定回调都在 UI 线程跑，串行访问，无需额外锁。
struct App {
  AutoMuteEngine engine;
  approuter::AppRouter router;
  bool prepared = false;
  uint32_t pid = 0;
  std::string routeStatus; // 码（前端本地化）：routed/cable_missing/route_failed/unavailable
  std::string routeDetail; // 失败时原始诊断（未翻译）
};

std::string b(bool v) { return v ? "true" : "false"; }
std::string num(double v) {
  char buf[32];
  snprintf(buf, sizeof buf, "%.3f", v);
  return buf;
}

// req 是 JS 实参的 JSON 数组字符串，取第 idx 个位置参数。
std::string arg(const std::string& req, unsigned idx) {
  return json_parse(req, "", idx);
}

// %LOCALAPPDATA%\AutoMute（确保存在）：放 targets.bin / lang.txt 等。
std::string amDir() {
  const char* base = getenv("LOCALAPPDATA");
  std::string dir = base ? base : ".";
  dir += "\\AutoMute";
  CreateDirectoryA(dir.c_str(), nullptr);
  return dir;
}
// 读持久化的界面语言（仅接受 zh/en，否则空=按系统语言猜）。
std::string readLang() {
  std::ifstream f(amDir() + "\\lang.txt");
  std::string s;
  if (f)
    std::getline(f, s);
  return (s == "zh" || s == "en") ? s : std::string();
}

std::string listAppsJson() {
  std::string out = "[";
  bool first = true;
  for (auto& s : listAudioSessions()) {
    if (!first)
      out += ",";
    first = false;
    out += "{\"pid\":" + std::to_string(s.pid) + ",\"name\":" +
           json_escape(s.name) + ",\"active\":" + b(s.active) +
           ",\"device\":" + json_escape(s.device) + "}";
  }
  return out + "]";
}

std::string statusJson(App& app) {
  std::string t = "[";
  size_t n = app.engine.targetCount();
  for (size_t i = 0; i < n; ++i) {
    auto v = app.engine.targetAt(i);
    if (i)
      t += ",";
    t += "{\"name\":" + json_escape(v.name) + ",\"sim\":" + num(v.similarity) +
         ",\"muted\":" + b(v.muted) + "}";
  }
  t += "]";
  return "{\"running\":" + b(app.engine.running()) + ",\"muted\":" +
         b(app.engine.muted()) + ",\"similarity\":" +
         num(app.engine.similarity()) + ",\"routed\":" +
         b(app.router.routed()) + ",\"routeStatus\":" +
         json_escape(app.routeStatus) + ",\"routeDetail\":" +
         json_escape(app.routeDetail) + ",\"targets\":" + t + "}";
}

// 无边框窗口（Uw）：子类化 webview 的窗口过程。保留 WS_OVERLAPPEDWINDOW 风格
// （Aero Snap/最小化最大化动画/任务栏都正常），仅吃掉非客户区(原生标题栏/边框)
// 并自己重建缩放热区。拖拽走 JS winDrag（WM_NCLBUTTONDOWN/HTCAPTION），不在此
// 硬编码标题栏几何。其余消息一律转回 webview 原 proc（DefSubclassProc）。
LRESULT CALLBACK frameProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR,
                           DWORD_PTR) {
  switch (msg) {
  case WM_NCCALCSIZE:
    if (wp) {
      // 客户区铺满整窗（移除原生标题栏/边框）。最大化时留出边框，否则内容被
      // 屏幕边缘裁掉、任务栏被盖。
      if (IsZoomed(hwnd)) {
        auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
        int fx = GetSystemMetrics(SM_CXSIZEFRAME) +
                 GetSystemMetrics(SM_CXPADDEDBORDER);
        int fy = GetSystemMetrics(SM_CYSIZEFRAME) +
                 GetSystemMetrics(SM_CXPADDEDBORDER);
        p->rgrc[0].left += fx;
        p->rgrc[0].right -= fx;
        p->rgrc[0].top += fy;
        p->rgrc[0].bottom -= fy;
      }
      return 0; // 不调默认 = 不减边框
    }
    break;
  case WM_NCHITTEST: {
    // 仅重建四边/四角缩放热区；其余交客户区（HTML 标题栏 + winDrag 拖拽）。
    const LONG b = 8;
    POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    RECT rc;
    GetWindowRect(hwnd, &rc);
    bool L = pt.x < rc.left + b, R = pt.x >= rc.right - b;
    bool T = pt.y < rc.top + b, B = pt.y >= rc.bottom - b;
    if (T && L) return HTTOPLEFT;
    if (T && R) return HTTOPRIGHT;
    if (B && L) return HTBOTTOMLEFT;
    if (B && R) return HTBOTTOMRIGHT;
    if (L) return HTLEFT;
    if (R) return HTRIGHT;
    if (T) return HTTOP;
    if (B) return HTBOTTOM;
    return HTCLIENT;
  }
  }
  return DefSubclassProc(hwnd, msg, wp, lp);
}

// 把 HWND 改成无边框 + 装阴影。
void makeFrameless(HWND hwnd) {
  SetWindowSubclass(hwnd, frameProc, 1, 0);
  MARGINS m{0, 0, 0, 1};
  DwmExtendFrameIntoClientArea(hwnd, &m); // 无边框也有投影
  // Win11 圆角（DWMWA_WINDOW_CORNER_PREFERENCE=33 / DWMWCP_ROUND=2）；Win10 静默忽略。
  DWORD corner = 2;
  DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

} // namespace

int main() {
  SetConsoleOutputCP(CP_UTF8);
  approuter::AppRouter::recoverStaleRoutes(); // 清上次崩溃残留（自带 COM init/uninit）

  // ⚠️ 先建 webview：它给 UI 线程初始化 STA 套间。AppRouter 随后建时其
  //    RoInitialize(MTA) 会返回 RPC_E_CHANGED_MODE（已容忍，不强占套间），
  //    避免"先 MTA 后 STA"让 webview 抛 CoInitializeEx concurrency 冲突。
  webview::webview w(true, nullptr);
  w.set_title("AutoMute");
  w.set_size(560, 640, WEBVIEW_HINT_NONE);
  w.set_size(420, 520, WEBVIEW_HINT_MIN); // 最小尺寸，防拉小破版

  // Uw：无边框 + 自绘标题栏。拿 webview 的 HWND 子类化。
  HWND hwnd = static_cast<HWND>(w.window().value());
  makeFrameless(hwnd);

  App app;

  // 加载模型（零目标启动，靠在线抓取登记）。
  AutoMuteEngine::Config cfg;
  cfg.targetWav = ""; // GUI 走在线抓取，不预登记
  std::string err;
  app.prepared = app.engine.prepare(cfg, err);
  std::string prepareErr = app.prepared ? "" : err;

  // 持久化：声纹名单存 %LOCALAPPDATA%\AutoMute\targets.bin，启动自动读回。
  app.engine.setPersistPath(amDir() + "\\targets.bin");
  std::string lerr;
  app.engine.loadTargets(lerr); // 失败(文件损坏)忽略，当空名单

  w.bind("listApps",
         [](const std::string&) -> std::string { return listAppsJson(); });

  w.bind("cableStatus", [&app](const std::string&) -> std::string {
    std::string id;
    bool installed = approuter::cableInstalled(id);
    return "{\"available\":" + b(app.router.available()) +
           ",\"installed\":" + b(installed) + "}";
  });

  // 注：绑定名避开浏览器内置 window.* 方法（stop/open/close/focus/print…）——
  // 否则 JS 调到的是内置方法而非我们的绑定（window.stop 撞过坑）。
  w.bind("startEngine", [&app](const std::string& req) -> std::string {
    if (!app.prepared)
      return "{\"ok\":false,\"error\":\"model\"}";
    uint32_t pid = (uint32_t)std::stoul("0" + arg(req, 0));
    if (pid == 0)
      return "{\"ok\":false,\"error\":\"pid\"}";
    // 自动路由（失败不挡启动，回退引导手动）。只设码 + 诊断，前端本地化。
    app.routeStatus.clear();
    app.routeDetail.clear();
    if (app.router.available()) {
      std::string id, e;
      if (!approuter::cableInstalled(id))
        app.routeStatus = "cable_missing";
      else if (app.router.route(pid, id, e))
        app.routeStatus = "routed";
      else {
        app.routeStatus = "route_failed";
        app.routeDetail = e;
      }
    } else {
      app.routeStatus = "unavailable";
    }
    std::string e;
    if (!app.engine.start(pid, e)) {
      app.router.restore();
      return "{\"ok\":false,\"error\":\"engine\",\"detail\":" + json_escape(e) +
             "}";
    }
    app.pid = pid;
    return "{\"ok\":true}";
  });

  w.bind("stopEngine", [&app](const std::string&) -> std::string {
    app.engine.stop();
    app.router.restore();
    app.routeStatus.clear();
    app.routeDetail.clear();
    return "{\"ok\":true}";
  });

  w.bind("capture", [&app](const std::string& req) -> std::string {
    std::vector<float> snap;
    uint32_t sr = 0;
    if (!app.engine.snapshotRecent(3.0f, snap, sr) || snap.empty())
      return "{\"ok\":false,\"error\":\"no_audio\"}";
    std::string name = arg(req, 0);
    if (name.empty())
      name = "Target " + std::to_string(app.engine.targetCount() + 1);
    std::string e;
    int idx = app.engine.addTarget(name, snap, sr, e);
    if (idx < 0)
      return "{\"ok\":false,\"error\":\"enroll\",\"detail\":" + json_escape(e) +
             "}";
    return "{\"ok\":true,\"idx\":" + std::to_string(idx) + "}";
  });

  w.bind("setMuted", [&app](const std::string& req) -> std::string {
    size_t idx = (size_t)std::stoul("0" + arg(req, 0));
    bool on = arg(req, 1) == "true";
    app.engine.setTargetMuted(idx, on);
    return "{\"ok\":true}";
  });

  w.bind("renameTarget", [&app](const std::string& req) -> std::string {
    size_t idx = (size_t)std::stoul("0" + arg(req, 0));
    std::string name = arg(req, 1);
    if (!name.empty())
      app.engine.renameTarget(idx, name);
    return "{\"ok\":true}";
  });

  w.bind("removeTarget", [&app](const std::string& req) -> std::string {
    size_t idx = (size_t)std::stoul("0" + arg(req, 0));
    app.engine.removeTarget(idx);
    return "{\"ok\":true}";
  });

  w.bind("clearTargets", [&app](const std::string&) -> std::string {
    app.engine.clearTargets();
    return "{\"ok\":true}";
  });

  w.bind("getStatus",
         [&app](const std::string&) -> std::string { return statusJson(app); });

  // 持久化界面语言选择（zh/en）。
  w.bind("setLang", [](const std::string& req) -> std::string {
    std::string code = arg(req, 0);
    if (code == "zh" || code == "en") {
      std::ofstream f(amDir() + "\\lang.txt", std::ios::trunc);
      if (f)
        f << code;
    }
    return "{\"ok\":true}";
  });

  // ── Uw：窗口控制（自绘标题栏调用）──
  w.bind("winMinimize", [hwnd](const std::string&) -> std::string {
    ShowWindow(hwnd, SW_MINIMIZE);
    return "{}";
  });
  w.bind("winToggleMaximize", [hwnd](const std::string&) -> std::string {
    ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
    return "{}";
  });
  w.bind("winClose", [hwnd](const std::string&) -> std::string {
    PostMessage(hwnd, WM_CLOSE, 0, 0);
    return "{}";
  });
  // 标题栏拖拽：发起标准窗口移动（避开 WebView2 不默认支持 app-region:drag）。
  w.bind("winDrag", [hwnd](const std::string&) -> std::string {
    ReleaseCapture();
    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    return "{}";
  });
  // 边缘缩放：WebView2 子窗口盖住整窗→父窗口 NCHITTEST 不触发，故由 HTML 边缘
  // 手柄调此发起标准缩放（ht 为 HTLEFT/HTTOP/HTBOTTOMRIGHT… 方向码）。
  w.bind("winResize", [hwnd](const std::string& req) -> std::string {
    int ht = std::stoi("0" + arg(req, 0));
    ReleaseCapture();
    SendMessage(hwnd, WM_NCLBUTTONDOWN, (WPARAM)ht, 0);
    return "{}";
  });
  w.bind("winIsMaximized", [hwnd](const std::string&) -> std::string {
    return IsZoomed(hwnd) ? "true" : "false";
  });

  // 前端页面（内嵌，避免运行时找文件）。启动前把"保存的语言"和"模型错误"
  // 注入成全局变量，前端读出来用（__lang 决定初始界面语言）。
  extern const char* kIndexHtml;
  std::string inject;
  if (std::string lang = readLang(); !lang.empty())
    inject += "window.__lang=" + json_escape(lang) + ";";
  if (!prepareErr.empty())
    inject += "window.__prepareErr=" + json_escape(prepareErr) + ";";
  std::string html = inject.empty()
                         ? std::string(kIndexHtml)
                         : "<script>" + inject + "</script>" + kIndexHtml;
  w.set_html(html);
  w.run();

  app.engine.stop();
  app.router.restore();
  return 0;
}
