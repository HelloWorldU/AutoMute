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
  std::string routeMsg; // 最近一次路由结果提示
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
         b(app.router.routed()) + ",\"routeMsg\":" + json_escape(app.routeMsg) +
         ",\"targets\":" + t + "}";
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
  if (const char* base = getenv("LOCALAPPDATA")) {
    std::string dir = std::string(base) + "\\AutoMute";
    CreateDirectoryA(dir.c_str(), nullptr);
    app.engine.setPersistPath(dir + "\\targets.bin");
    std::string lerr;
    app.engine.loadTargets(lerr); // 失败(文件损坏)忽略，当空名单
  }

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
      return "{\"ok\":false,\"msg\":\"模型未加载\"}";
    uint32_t pid = (uint32_t)std::stoul("0" + arg(req, 0));
    if (pid == 0)
      return "{\"ok\":false,\"msg\":\"无效 PID\"}";
    // 自动路由（失败不挡启动，回退引导手动）。
    app.routeMsg = "";
    if (app.router.available()) {
      std::string id, e;
      if (!approuter::cableInstalled(id))
        app.routeMsg = "未装 VB-CABLE，请手动路由或安装";
      else if (app.router.route(pid, id, e))
        app.routeMsg = "已自动路由到 VB-CABLE";
      else
        app.routeMsg = "自动路由失败，请手动设置：" + e;
    } else {
      app.routeMsg = "本机不支持自动路由，请手动把该 App 设为 CABLE Input";
    }
    std::string e;
    if (!app.engine.start(pid, e)) {
      app.router.restore();
      return "{\"ok\":false,\"msg\":" + json_escape("启动失败：" + e) + "}";
    }
    app.pid = pid;
    return "{\"ok\":true,\"msg\":" + json_escape(app.routeMsg) + "}";
  });

  w.bind("stopEngine", [&app](const std::string&) -> std::string {
    app.engine.stop();
    app.router.restore();
    app.routeMsg = "";
    return "{\"ok\":true}";
  });

  w.bind("capture", [&app](const std::string& req) -> std::string {
    std::vector<float> snap;
    uint32_t sr = 0;
    if (!app.engine.snapshotRecent(3.0f, snap, sr) || snap.empty())
      return "{\"ok\":false,\"msg\":\"还没抓到声音，等目标出声再试\"}";
    std::string name = arg(req, 0);
    if (name.empty())
      name = "目标" + std::to_string(app.engine.targetCount() + 1);
    std::string e;
    int idx = app.engine.addTarget(name, snap, sr, e);
    if (idx < 0)
      return "{\"ok\":false,\"msg\":" + json_escape(e) + "}";
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

  // 前端页面（内嵌，避免运行时找文件）。模型加载失败也照常显示，给出提示。
  extern const char* kIndexHtml;
  std::string html = kIndexHtml;
  if (!prepareErr.empty()) {
    // 把错误塞进页面一个全局变量，前端读出来提示。
    html = "<script>window.__prepareErr=" + json_escape(prepareErr) +
           ";</script>" + html;
  }
  w.set_html(html);
  w.run();

  app.engine.stop();
  app.router.restore();
  return 0;
}
