# 产品化路线：从"能跑的核心"到"别人能直接用"

> 背景见 [`../DESIGN.md`](../DESIGN.md) 的"产品化与音频路由"。
> 核心矛盾：同设备 loopback 抓取+回放 = 数字反馈，污染识别。抓取源必须 ≠ 回放目标。
> 选定路线：进程 loopback（按进程抓取，干净、天然无反馈）。
> ⚠️ 原以为可“无需虚拟声卡”，真机验证证伪——隔离原声仍需虚拟声卡路由（见 P2 节）。

## 步骤

| # | 目标 | 产出 | 状态 |
|---|------|------|------|
| P1 | ~~**设备选择**~~ | ~~LoopbackCapture/RenderPlayback 可指定设备；`--list/--in/--out`；配 VB-CABLE 解决反馈~~ | 🗑️ 已移除（2026-06-02）：被 P2 进程 loopback 取代，`audio_devices` 删除、`initialize()` 回退无参 |
| P2 | **进程 loopback** | 用 Win10 process-loopback API 按进程抓目标 App；静音该 App 直接输出；渲染处理后版本。无需虚拟声卡 | ✅ 全子步骤完成，接入主程序 `automute_app`（真机验证待做） |
| P3-P5 | **并入 M4「app 壳」** | 配置持久化 + enroll UX + GUI 外壳，已经 `/create` 对齐成 v1 spec | 见 [`m4-app-shell.md`](m4-app-shell.md) |

> P3/P4/P5 已收敛进 M4：enroll = **边看边圈在线抓取**（非事先录音）；GUI 框架定 **Dear ImGui+GLFW+OpenGL3**（非 Qt/Tauri）；持久化推到 v1.1。详见 M4 spec。

## P1 设备选择（已移除）

P1 是当初为「抓取源≠回放目标、配虚拟声卡消反馈」铺的地基。P2 进程 loopback 落地后，
反馈问题从根上消失（只抓目标进程、不抓自己的回放），P1 的设备选择不再需要：
- 删除 `audio_devices.{h,cpp}`；
- `LoopbackCapture::initialize` / `RenderPlayback::initialize` 回退为无参（只用默认设备）；
- `automute_app` 去掉 `--list/--in/--out`，改用 `--apps` + `--proc <PID>`。

## P2 进程 loopback（当前产品形态）

关键 API：`ActivateAudioInterfaceAsync` + `AUDIOCLIENT_ACTIVATION_PARAMS`
（`PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE`）按 PID 抓取单个进程音频。

### ⚠️ 重要修正（2026-06-02 真机验证）：mute 不能用，省不掉虚拟声卡

原计划"按进程设静音（`ISimpleAudioVolume::SetMute`）隔离原声 → 彻底摆脱虚拟声卡"**不成立**。
真机实测：对正在出声的 chrome(PID 14520)，**不静音**时探针电平 -11.8dBFS 正常跳动；
主程序里一旦 `SetMute` 同一进程，loopback **立即变 -120dB**。
原因：**进程 loopback 抽头位于"会话静音节点"的下游**——一静音，流到抽头时已是零。
推论：你不能用一个在执行器下游的传感器去闭环控制那个执行器。

→ **隔离原声只能靠路由**：把目标 App 输出路由到虚拟声卡(用户听不到)，我们不静音地抓它、
门控后渲染到喇叭。好处是**不需要把 P1 的 `--in/--out` 加回来**——Windows 自带「每应用输出路由」
即可，系统默认仍是喇叭，我们渲染到默认即喇叭。

子步骤：
- ✅ P2.1 spike：MinGW 手补 `AUDIOCLIENT_ACTIVATION_PARAMS` 等缺失声明；
  `ProcessLoopbackCapture::initialize(pid)` 异步激活成功；`automute_ploop_probe <PID>` 验证抓取。
- ✅ P2.2 进程选择器：`audio_sessions` 枚举会话列出"进程 + PID + 是否出声"；`automute_app --apps` 验证（QQMusic/chrome）。
- 🗑️ P2.3 静音目标直接输出：`setProcessMuted` 已实现并真机验证——**会同时掐死 loopback 抓取，不可用，已移除**（见上）。
- ✅ P2.4 接主程序：`automute_app --proc <PID>` 用进程 loopback 抓目标 App → 声纹判定 → 渲染门控版到默认设备。
  **不再静音目标 App**；隔离原声由用户把目标 App 路由到虚拟声卡完成。
  **真机端到端验证通过**（2026-06-02，chrome 路由到 VB-CABLE）：登记 spkA 为目标，
  chrome 循环放 spkA 另一段 → 相似度 ~0.7 过阈 → 🔇 喇叭静音；放异人 spkB → ~0.1 → 🔊 放行。
- 🔧 配套修复：`--apps` 改为扫描**所有活动输出设备**并显示每个会话"输出到哪个设备"——
  否则路由到虚拟声卡的进程会从只看默认设备的列表里消失，找不到 PID。
