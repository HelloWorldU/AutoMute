# STATUS

> **功能实现状态单一事实源**。Agent 启动或遗忘上下文时先查此表。
> 实现状态一变，此文档必须同步。深细节交给各 exec-plan，这里只记状态 + 一句话 + 指针。
> 图例：✅ 已实现可运行 / 🚧 框架占位待填 / ❌ 未实现 / 🅿️🛑 暂停或否定。

## 里程碑

| # | 目标 | 状态 |
|---|------|------|
| **M1** | C++ 实时音频脊梁：WASAPI loopback 抓系统声音→原样回放，测延迟，手动静音 | ✅ 闭环，稳态 ~20ms |
| **M2** | 声纹识别：录入样本→ONNX embedding→实时比对→命中即掐 | ✅ 真机验通，详见 [`exec-plans/m2-speaker-id.md`](exec-plans/m2-speaker-id.md) |
| **M3** | 调优：压短判断窗、VAD、阈值 | 🅿️ 暂停：窗长地板≈1.5s（缩窗反噬，M3.4）；VAD/迟滞边际小暂缓 |
| **M4** | app 壳：引擎解耦 + 在线抓取 + 自动路由 + GUI | ✅ **v1 单人闭环真机验通**；仅余配置/声纹持久化→v1.1。详见 [`m4-app-shell.md`](exec-plans/m4-app-shell.md) |
| **UI 打磨** | 前端迁 Vue 3 + Naive UI、无边框现代窗口、精致暗色 | ✅ U0/U1/Uw/U2/U4，详见 [`ui-polish.md`](exec-plans/ui-polish.md) |

## M4 子步骤（详见 [`m4-app-shell.md`](exec-plans/m4-app-shell.md)）

| # | 目标 | 状态 |
|---|------|------|
| M4.1 | 抽 `AutoMuteEngine`（核心与界面解耦，CLI 改薄前端） | ✅ `engine.{h,cpp}`，行为不变 |
| M4.2 | 引擎扩展：多目标（按 N 设计，v1=1）+ 最近 N 秒快照 API | ✅ 多目标(聚合 max + per-target) + `snapshotRecent()`(抓取线程 5s mono 历史环)；GUI 真机验过 |
| M4.3 | 路由模块：未公开 `IAudioPolicyConfig` 自动路由/还原 + 兜底 | ✅ 独立 `AppRouter`；真机 + 崩溃 journal 兜底验通 |
| M4.4 | GUI 外壳（C++ webview/WebView2） | ✅ 真机单人闭环（选 App→路由→抓取命名→相似度拉开→掐声）。后迁 Vue（见 UI 打磨） |

## 核心模块

| 模块 | 状态 | 一句话 | 关键文件 |
|------|------|--------|----------|
| 音频抓取（WASAPI loopback） | ✅ | 48kHz/2ch float，电平实时跳动 | `audio/loopback_capture.cpp` |
| 实时管线（无锁 SPSC 环形缓冲 + 双线程） | ✅ | 闭环稳态 ~20ms | `audio/ring_buffer.h`, `audio/render_playback.cpp` |
| 静音门控 gate | ✅ | 渲染端按 muted 标志输出静音 | `audio/render_playback.cpp` |
| 进程 loopback 抓取（P2.1） | ✅ | 按 PID 抓单进程音频（MinGW 手补 API） | `audio/process_loopback.{h,cpp}` |
| 进程会话枚举（P2.2） | ✅ | `--apps` 扫所有活动输出设备列会话（含路由到虚拟声卡的） | `audio/audio_sessions.{h,cpp}` |
| 特征提取 fbank + 声纹推理 + Embedder | ✅ | wav→fbank(80)→ECAPA→192 维；同人 0.835/异人 0.07；含 48k→16k 抗混叠重采样 | `audio/embedder.{h,cpp}`, `audio/wav_io.cpp`, `audio/resampler.{h,cpp}` |
| **引擎 AutoMuteEngine（M4.1+M4.2）** | ✅ | 抓取+渲染+分析三线程核心，与界面解耦：`prepare→start→similarity/muted→stop`；多目标 + `snapshotRecent`。CLI/GUI 共用 | `engine.{h,cpp}` |
| **每应用路由 AppRouter（M4.3）** | ✅ | 与引擎解耦。检测 CABLE + 未公开 `IAudioPolicyConfigFactory` route/restore（双 role）+ RAII 还原 + 崩溃 journal 兜底 + ClearAll。⚠️ 接口类型须外部链接（否则 GCC -O2 去虚化跳飞）；运行中用户手改设备会冲突（v1 不监听，backlog） | `audio/app_router.{h,cpp}` |
| **CLI 前端 `automute_app`（P2.4）** | ✅ | `--proc <PID>` 驱动引擎的薄 CLI；调参旋钮 + 热键 c/1-9 冰测；引擎诊断入口 | `app_main.cpp` |
| **GUI `automute_gui`（M4.4 + UI 打磨）** | ✅ | C++ webview 宿主 + **Vue 3 + Naive UI** 前端（`frontend/`→Vite 构建单文件→`scripts/embed-frontend.mjs` 嵌入 `kIndexHtml`）。无边框自绘标题栏、精致暗色、生动仪表、**中英 i18n**（vue-i18n + Naive locale，标题栏切换、`setLang` 持久化、后端只返回码前端本地化） | `gui_main.cpp`, `gui_html.cpp`(生成), `frontend/` |
| **声纹名单持久化（v1.1）** | ✅ | 引擎 `setPersistPath/loadTargets/clearTargets`，增删改/切开关自动存盘（二进制 `%LOCALAPPDATA%\AutoMute\targets.bin`）、启动自动加载；GUI 加「清空」。免每次重抓 | `automute/engine.{h,cpp}` |
| **测试（doctest + ctest）** | ✅ | Tier1 单元(环形缓冲/历史环/重采样, 224 断言) + Tier2 集成(声纹管线 同人 0.83/异人 0.03 + 名单持久化往返) + Tier3 冒烟脚本。详见 [`testing.md`](testing.md) | `tests/`, `automute/audio/history_ring.h` |
| VAD + 阈值迟滞 | ❌ | M3.2/M3.3，边际小暂缓 | — |

## 关键约束 / 踩坑（一处汇总，避雷）

- **窗长地板≈1.5s**：本 ECAPA 模型缩到 ≤0.75s 同人相似度塌缩；头部泄漏≈识别下限，非调参能消（要更短得换短时模型，超出 M3）。已加 `--window/--hop/--thresh` 旋钮。
- **隔离原声靠路由非静音**：`SetMute` 会连 loopback 抽头一起掐死（P2.3 移除）；改路由目标 App 到虚拟声卡。
- **未公开 COM 接口类型必须外部链接**：放匿名命名空间 → GCC -O2 去虚化到自生成 vtable → 调真实 COM 跳飞段错误。
- **webview/前端命名**：JS 函数名不得撞 C++ 绑定（`capture`）、绑定名不得撞浏览器内置（`stop`→`stopEngine`）。
- **轮询用 setTimeout 自调度**（非 setInterval）：否则异步轮询无背压、重叠雪崩烧 CPU。
- **套间**：先建 webview（占 STA）再建 AppRouter（MTA 拿 CHANGED_MODE 容忍），否则 webview 抛 CoInitializeEx 冲突。
- **WebView2 盖满窗口**：无边框拖拽/缩放走 JS→C++（`winDrag`/`winResize` 发 `WM_NCLBUTTONDOWN`），父窗口边缘 hit-test 收不到。

---

*产品形态 = 进程 loopback + 虚拟声卡路由隔离原声。**M4 v1 单人直播真机端到端闭环**。
**v1.1 进行中**：多人 UX ✅ + 声纹名单持久化 ✅ + 中英 i18n ✅（真机多人/切语言待验）。
余下可选：M3.2 VAD / M3.3 阈值迟滞（多人时更值得）/ 掐声淡入淡出 / 换短时模型缩短头部泄漏。*
