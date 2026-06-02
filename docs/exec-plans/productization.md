# 产品化路线：从"能跑的核心"到"别人能直接用"

> 背景见 [`../DESIGN.md`](../DESIGN.md) 的"产品化与音频路由"。
> 核心矛盾：同设备 loopback 抓取+回放 = 数字反馈，污染识别。抓取源必须 ≠ 回放目标。
> 选定路线：目标档2（进程 loopback，无需虚拟声卡），档1（设备选择）作地基。

## 步骤

| # | 目标 | 产出 | 状态 |
|---|------|------|------|
| P1 | **设备选择** | LoopbackCapture/RenderPlayback 可指定设备（非默认）；`--list` 列设备；`--in/--out` 选设备。配 VB-CABLE 即可解决反馈 | ✅ 枚举+选择跑通（`audio_devices`），`--list/--in/--out` 已接 |
| P2 | **进程 loopback** | 用 Win10 process-loopback API 按进程抓目标 App；静音该 App 直接输出；渲染处理后版本。无需虚拟声卡 | 🚧 spike 通过：MinGW 手补 API + 按 PID 激活/抓取成功（`process_loopback`）。剩：进程选择器、静音目标输出、接主程序 |
| P3 | 配置持久化 | 记住用户选的设备/进程/阈值（写配置文件） | ❌ |
| P4 | enroll 的 UX | 让用户"给一段目标的声音"：录制一段 / 从流里截一段（命令行→GUI） | ❌ |
| P5 | GUI 外壳（远期） | Qt / Tauri 界面：选 App、enroll、开关、状态显示 | ❌ |

## P1 设备选择（当前）

- `audio_devices`：枚举活动的输出(render)设备 + 友好名，供 `--list` 展示。
- `LoopbackCapture::initialize(deviceIndex=-1)`：-1 用默认；否则枚举集合取第 index 个。
- `RenderPlayback::initialize(deviceIndex=-1)`：同上。
- `automute_app`：`--list` 列设备并退出；`--in N` 抓取设备；`--out N` 渲染设备。

**用法（配 VB-CABLE 验证无反馈）**：
1. 装 VB-CABLE，把播放器/直播 App 输出设到 "CABLE Input"。
2. `automute_app --list` 看设备号。
3. `automute_app --in <CABLE Output 的号> --out <真实喇叭的号> 目标.wav`
4. 抓的是虚拟声卡、放的是喇叭 → 无反馈 → 识别恢复正常。

## P2 进程 loopback（目标架构，下一步）

关键 API：`ActivateAudioInterfaceAsync` + `AUDIOCLIENT_ACTIVATION_PARAMS`
（`PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE`）按 PID 抓取单个进程音频；
配合按进程设音量为 0（`ISimpleAudioVolume`）静音其直接输出。
做完即"装上选个 App 就能用"，彻底摆脱虚拟声卡。

子步骤：
- ✅ P2.1 spike：MinGW 手补 `AUDIOCLIENT_ACTIVATION_PARAMS` 等缺失声明；
  `ProcessLoopbackCapture::initialize(pid)` 异步激活成功；`automute_ploop_probe <PID>` 验证抓取。
- ❌ P2.2 进程选择器：枚举音频会话（`IAudioSessionManager2`）列出"正在出声的进程 + PID + 名字"供选。
- ❌ P2.3 静音目标直接输出：`ISimpleAudioVolume` 把目标进程会话设静音（注意验证是否影响 loopback 抓取）。
- ❌ P2.4 接主程序：进程 loopback 抓取替换设备 loopback；渲染到喇叭；无反馈。
