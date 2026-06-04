# M4「app 壳」spec —— 从 CLI 到能用的桌面应用

> 经 `/create` 对齐并批准（2026-06-04）。背景见 [`productization.md`](productization.md)（P3/P4/P5 在此合并为 M4）。
> 本文是 M4 的**需求事实源**；实现状态以 [`../STATUS.md`](../STATUS.md) 为准。
> UI 细节（控件布局、交互文案）留待各子步动手前再深度对齐，本文先钉**机制与边界**。

## 产品

桌面 GUI 工具：看直播时，把**某个命名的人**的声音实时掐掉，其余照常。
GUI 驱动已有的 [`AutoMuteEngine`](../../automute/engine.h)（M4.1 已抽出）。

## 对齐记录（决策 + 为什么）

| 决策点 | 选定 | 为什么 |
|--------|------|--------|
| **登记/命名一个人** | **边看边圈（在线抓取）** | 直播里人事先不知道；点「抓取当前说话人」→ 取最近 ~3s → 算声纹命名。踩中原始设计「轮流说话不重叠」前提，抓取片段天然单人 |
| **路由（设置+还原）** | **程序自动路由 + 兑底引导** | 每应用路由在 Win 持久 → 不还原则关 app 后目标 App 变哑巴，故"退出还原"必须；用未公开 `IAudioPolicyConfig` COM，失败回退引导手动 |
| **GUI 框架** | **C++ webview（嵌入 WebView2）** | 后台仍 C++（直接调引擎）、前端 HTML/CSS/JS 颜值天花板最高；比 Tauri 少一层 Rust。用 `webview/webview` 小库包 WebView2 |
| **v1 范围** | **垂直切片：全链路先单人** | 一条龙跑通（GUI↔引擎↔路由↔登记），多人只是"列表变 N"，紧接着扩。风险最低 |

## v1 范围（单人全链路）

1. **选 App**：下拉列出在出声的进程（PID + 当前输出设备，复用 `audio_sessions`）。
2. **自动路由**：选定后把该 App 输出路由到 VB-CABLE；没装 → 「一键安装」按钮；路由 API 失败 → 弹引导手动。
3. **边看边圈登记**：某人说话时点「抓取当前说话人」→ 引擎交出最近 ~3s → 算声纹 → 命名保存（v1 先 1 人）。
4. **一键静音**：该人一个静音开关；开 → 引擎检测到他说话就掐。
5. **实时状态**：相似度仪表 + 🔇/🔊。
6. **退出还原**：关 app 自动把该 App 路由改回原设备。

## 要新建/扩展的部件

- **引擎扩展**：① 暴露"最近 N 秒快照"（在线抓取用）；② 单目标→**多目标**（比对一组命名声纹，任一"开了静音"的人命中就掐）——v1 列表长度=1，但按 N 设计。
- **路由模块**：未公开 `IAudioPolicyConfig` COM（MinGW 手补声明）设置/还原每应用输出；+ VB-CABLE 检测与安装拉起；+ 失败兑底引导。
- **GUI 外壳**：C++ 应用嵌入 WebView2（`webview/webview` 库）；前端 HTML/CSS/JS；JS↔C++ 绑定暴露 listApps/start/capture/setMuted，前端轮询 similarity/muted 画仪表。

## 已知约束与风险（已摊开）

- 🧱 **~1.5s 识别地板**：认人/切换仍有 ~1~1.5s 头部泄漏，调参消不掉（M3.4 实测）。
- ⚠️ **路由 COM 未公开**：可能随 Win 版本变 → 必须有"引导手动"兑底。
- 📦 **WebView2 + MinGW（早验）**：WebView2 SDK 为 MSVC 设计，MinGW 下经 `webview/webview` 接入需先验通构建（类比 ORT / 进程 loopback 的 MinGW 适配）；运行时依赖 WebView2 Runtime（Win10/11 多已预装，否则引导安装）。
- 🎙️ **抓取假设轮流说话不重叠**（与原始设计一致）；抓取片段需 ~1.5-3s 单人语音才准。

## 不在 v1（→ v1.1 / v2）

- 多人名单（N 个人）—— v1 单人跑通后紧接着扩（列表变 N）。
- 配置/声纹**持久化**（在线抓取每次现登记，v1 可不存）。
- **自动分人**（diarization，研究级）。
- **实时设备监听**：运行中用户在「音量合成器」手动改目标 App 设备，会与我们的
  自动路由/还原冲突（还原时拿开始记的原值覆盖用户新选）。`IMMNotificationClient`
  对"每应用路由变化"无干净通知，价值不对等 → v1 假设运行中不手动乱动，记 backlog。

## 落地顺序（子步）

| # | 目标 | 状态 |
|---|------|------|
| M4.1 | 抽 `AutoMuteEngine`（核心与界面解耦） | ✅ 已完成 |
| M4.2 | 引擎扩展：多目标（按 N 设计，v1=1）+ "最近 N 秒"快照 API | ✅ 已完成（见下「M4.2 对齐」；真机多目标待用户验） |
| M4.3 | 路由模块：自动路由/还原（未公开 COM）+ VB-CABLE 检测/安装 + 兑底引导 | ✅ 已完成（a 检测 / b 路由还原 / c 接进 app_main；真机端到端 + 崩溃兜底验通） |
| M4.4 | GUI 外壳（C++ webview / WebView2）：前端 HTML/CSS/JS，把选 App/抓取命名/静音开关/仪表/退出还原串起来 | 🚧 骨架+地基验通：MinGW+WebView2 编译/链接/起窗口✓；前端单页 + 6 个 bind + 引擎/路由接通；启动存活验证✓。真机交互/听感待用户验 |

## M4.4 依赖获取（webview，gitignore 不入库，需手动 vendor）

`/third_party/` 被 gitignore（同 onnxruntime/kaldi）。webview 这样弄进来：

```bash
# 1) webview 0.12.0 单头（发布版就是单 webview.h；master 才拆多头）
curl -sL https://github.com/webview/webview/archive/refs/tags/0.12.0.tar.gz | tar -xz -C /tmp
cp /tmp/webview-0.12.0/core/include/webview/webview.h third_party/webview/include/
# 2) WebView2 SDK 头（从 nuget .nupkg 解出，放 webview.h 同目录供其 #include "WebView2.h"）
curl -sL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2792.45 -o /tmp/wv2.nupkg
unzip -j /tmp/wv2.nupkg "build/native/include/WebView2.h" \
  "build/native/include/WebView2EnvironmentOptions.h" -d third_party/webview/include/
```

MinGW 关键点（已验）：webview 默认 `MSWEBVIEW2_BUILTIN_IMPL`+`EXPLICIT_LINK` →
**无需 .lib、无需 WebView2Loader.dll**（运行时已装即可）；只链 win 系统库
（runtimeobject/ole32/oleaut32/version/shlwapi/shell32/gdi32/user32）。
**套间坑**：先建 webview（占 STA）再建 AppRouter，否则 router 的 MTA 让 webview 抛
`CoInitializeEx concurrency` 冲突。

## M4.2 对齐（已钉，2026-06-04）

动手前敲定的四项决策 + 为什么：

| 决策点 | 选定 | 为什么 |
|--------|------|--------|
| **快照来源** | 抓取线程另维护"最近 N 秒 mono 历史环"，`std::mutex`+`std::vector` 加锁拷贝 | `rbAna_` 是 SPSC 单向流水线：read 破坏性消费、只准一个读者、会被分析线程读空 → 没法旁路偷看最近 N 秒。快照读者是第三条线程，必须另开历史环（不能复用 SPSC） |
| **快照时长 N** | **历史环存 5s / 默认抓 3s** | 1.5s 是「实时判定窗长地板」（延迟 vs 准确妥协，M3.4）；登记新目标不赶延迟，3s = 1.5s 地板 + 修边余量（头尾静音/半句削掉后仍要剩 ≥1.5s 真人声）。登记声纹是地基，糊了污染之后每次比对。5s 环留余量，点晚了还抓得到 |
| **多目标相似度对外** | **聚合 `similarity()` 取 max + per-target getters** | 保留现有 `float similarity()`（返回所有目标中最高）让 CLI 不破；另加 `targetCount()`/`targetAt(i){name,sim,muted}` 给 M4.4 GUI 画每人仪表。向后兼容、改动最小 |
| **新圈目标静音开关默认** | **默认关，需手动一键静音** | 对齐 spec 流程：步骤3「圈人命名」与步骤4「一键静音」是分开两步。圈完先看相似度仪表确认是对的人，再开掐 |
| **M4.2 冰测方式** | **CLI 加隐藏调试键** | `app_main` 加键（如 `c` 抓当前说话人→命名→加为目标，`t` 切静音开关），真机走一遍多目标+快照；不破现有 `--proc` 用法，GUI（M4.4）出来前先验通地基 |

快照接口草案：

```cpp
// 拷出最近 seconds 秒的单声道样本（在线圈人用）。运行中随时可调，
// 不影响分析流水线。返回实际拷出的样本（不足 seconds 就给现有的）。
bool snapshotRecent(float seconds, std::vector<float>& outMono, uint32_t& sr);
```

## 待深入对齐（动手前再钉）

- M4.4 前端栈：纯 HTML/CSS/JS（无构建步骤、轻）vs 框架（Svelte/React，需 Node 构建）——倾向先纯前端。
- M4.4 JS↔C++ 绑定模型 + 实时状态推送（前端 setInterval 轮询 getter / C++ 定时 eval 推）。
- M4.4 UI 细节：主窗口布局、控件、交互文案、相似度仪表的呈现。
- M4.3 路由：`IAudioPolicyConfig` 的具体 CLSID/方法、还原时机（仅退出 vs 切换 App 时）。
