# STATUS

> **功能实现状态单一事实源**。Agent 每次启动或遗忘上下文时，先查此表。
> 任何实现状态变更必须同步更新此文档。

## 图例

| 符号 | 含义 |
|------|------|
| ✅ | 真实实现 — 代码已写，可运行 |
| 🚧 | 框架/占位 — 目录或接口已建，核心逻辑待填充 |
| ❌ | 未实现 — 仅存在于规格或 TODO 中 |

## 里程碑

| # | 目标 | 状态 |
|---|------|------|
| **M1** | 打通 C++ 实时音频脊梁：WASAPI loopback 抓系统声音 → 原样回放，测出端到端延迟，加手动静音开关 | ✅ 闭环跑通，管线缓冲稳定 ~20ms |
| M2 | 接入声纹识别：录入目标样本 → ONNX 推理 embedding → 实时比对 → 命中即掐 | ✅ 全子步骤完成，自动掐声真机验证通过（见 P2.4），详见 [`exec-plans/m2-speaker-id.md`](exec-plans/m2-speaker-id.md) |
| M3 | 调优：压短判断窗口、VAD 抗噪、阈值调参，让开头泄漏收窄 | 🅿️ 暂告段落：M3.1✅ 滑动窗；M3.4🛑 窗长地板≈1.5s（缩窗反噬）；VAD/迟滞边际小暂缓 |
| M4 | app 壳（产品化 P3+P4+P5）：配置持久化 + enroll UX + GUI | 🚧 v1 单人闭环真机验通（M4.1 引擎解耦 / M4.2 多目标+快照 / M4.3 自动路由 / M4.4 GUI 全部✅）。剩 UX 打磨 + v1.1 多人/持久化 |

## M4 子步骤

> 已经 `/create` 对齐并批准，详见 [`exec-plans/m4-app-shell.md`](exec-plans/m4-app-shell.md)。
> v1 = 垂直切片（全链路先单人，在线抓取登记 + 自动路由 + ImGui GUI）。

| # | 目标 | 状态 |
|---|------|------|
| M4.1 | 抽 `AutoMuteEngine`（核心与界面解耦）：`prepare(cfg)`→`start(pid)`→轮询 `similarity()/muted()`→`stop()`，引擎不做 UI 输出。CLI 改薄前端 | ✅ `automute/engine.{h,cpp}`，行为不变，冒烟通过 |
| M4.2 | 引擎扩展：单目标→多目标（按 N 设计，v1=1）+ 暴露"最近 N 秒快照"API（在线抓取登记的地基） | ✅ 引擎扩展完成：多目标列表（聚合 max+per-target getters，任一开静音命中即掐）+ `snapshotRecent()`（抓取线程维护 5s mono 历史环，加锁拷）+ 运行中 `addTarget`。CLI 加热键 c/1-9 冰测；构建通过、快照环形数学单测通过、`--apps` 跑通。真机多目标/在线抓取待用户验 |
| M4.3 | 路由模块：未公开 `IAudioPolicyConfig` COM 自动路由/还原每应用输出 + VB-CABLE 检测/安装 + 失败兑底引导 | ✅ 独立 `AppRouter` 模块（M4.3a 检测 + M4.3b 路由/还原 + M4.3c 接进 app_main）。真机端到端验证：自动路由 QQMusic→CABLE、引擎运行、强杀留 journal→下次启动 recoverStaleRoutes 恢复到扬声器、journal 清除。失败逐级回退引导手动 |
| M4.4 | GUI 外壳（C++ webview / WebView2，前端 HTML/CSS/JS）：选 App 下拉 + 边看边圈抓取命名 + 一键静音开关 + 相似度仪表/🔇🔊 + 退出还原 | ✅ **真机端到端验通**（单人直播闭环：选 chrome→自动路由 CABLE→在线抓取命名→相似度拉开 0.62 vs 异人→掐他生效）。webview 0.12 单头 vendor，6 个 bind 驱动引擎+路由，前端 250ms 轮询。**踩坑**：①先建 webview 占 STA 再建 AppRouter（否则套间冲突）②前端 JS 函数名不能与 C++ 绑定同名（`capture` 撞车致无限递归，已改 `onCapture`）③下拉按 PID 去重（路由后 Windows 留过期会话残骸）。真机也验到强杀后 recoverStaleRoutes 自动还原路由。UX 打磨：点目标名字原地改名（`renameTarget`）、✕ 删目标（`removeTarget`）——分析线程改为持锁比对，删除不悬空 |

## M3 子步骤

| # | 目标 | 状态 |
|---|------|------|
| M3.1 | 分析线程改滑动窗+跳步（不清空重攒）：每 HOP_SEC 对最近 WINDOW_SEC 重判一次，消除"目标横跨窗口被稀释→最坏 ~3s"、尾部更跟手 | ✅ 已接（`app_main.cpp` 调参旋钮 THRESH/WINDOW_SEC/HOP_SEC），真机体感待验 |
| M3.2 | VAD：静音/无人声段不参与判定（避免拿静音 embedding 误判） | ❌ |
| M3.3 | 阈值迟滞（开>0.5/关<0.4）+ 去抖，避免边界反复翻转 | ❌ |
| M3.4 | 缩短窗长（头部泄漏的真正杠杆） | 🛑 实测否定：缩窗反而漏。本模型实测 1.5s 同人~0.7 / 1.0s~0.55(margin薄) / ≤0.75s 同人相似度塌到~0.05→目标全程断续漏出。**窗长地板≈1.5s，保持默认不动**。头部泄漏≈本模型识别下限，非调参能消（要更短需换短时声纹模型，超出 M3，记入 backlog）。已加 `--window/--hop/--thresh` CLI 旋钮免重编译扫参 |

## M2 子步骤

| # | 目标 | 状态 |
|---|------|------|
| M2.1 | 集成 ONNX Runtime（预编译二进制）+ 跑通 dummy 推理，证明能链接能跑 | ✅ MinGW 直链 MSVC .lib 成功，Env 初始化通过（v1.26.0） |
| M2.2a | 加载真实模型 + 假数据跑通推理，确认 I/O 规格 | ✅ feats[B,T,80] → embs[B,192]，假输入推理成功 |
| M2.2b | 写 fbank 特征提取，喂真 wav → 得到真 embedding | ✅ wav→fbank(298×80)→模型→192维声纹，全管线通 |
| M2.3 | 用真实语音验证可区分性 + 封装 Embedder | ✅ 同人 0.835 / 异人 0.03~0.10，区分度极大，方案成立 |
| M2.4 | 实时分析线程：抓取 → 分析 ring buffer → 攒窗(1.5s) → Embedder → 余弦比对目标 → 判定(阈值 0.5) | ✅ 程序构建/enroll/双线程跑通；离线管线全验证；真机实时检测待用户播放目标语音确认 |
| M2.5 | 接线：判定结果驱动 gate，自动掐声 | ✅ 三线程合体(抓取+渲染带gate+分析)，检测目标→自动静音；构建/线程跑通，真机自动掐声待用户验 |

## 核心模块

| 模块 | 状态 | 说明 | 关键文件 |
|------|------|------|----------|
| 音频抓取（WASAPI loopback） | ✅ | 已验证：电平条随系统声音实时跳动，48kHz/2ch float | `automute/audio/loopback_capture.cpp` |
| 实时音频管线（环形缓冲 / 无锁队列 / 低延迟线程） | ✅ | 无锁 SPSC 环形缓冲 + 抓取/渲染双线程；闭环稳态 ~20ms | `automute/audio/ring_buffer.h`, `automute/audio/render_playback.cpp`, `automute/app_main.cpp` |
| 静音门控（gate） | ✅ | 渲染端按 muted 标志输出静音，由声纹判定自动触发 | `automute/audio/render_playback.cpp`, `automute/app_main.cpp` |
| 实时声纹检测 | ✅ | 抓取+分析双线程，1.5s 窗 enroll 比对，打印是否目标在说话 | `automute/detect_main.cpp` |
| **引擎 AutoMuteEngine（M4.1+M4.2）** | ✅ | 抓取+渲染+分析三线程的核心，与界面解耦：`prepare(cfg)`→`start(pid)`→`similarity()/muted()`→`stop()`；不做 UI 输出。CLI/GUI 共用。**M4.2**：多目标列表（`addTarget`/`setTargetMuted`/`targetCount`/`targetAt`，任一开静音命中即掐，聚合 `similarity()` 取 max）+ `snapshotRecent(sec)`（抓取线程维护 5s mono 历史环，加锁拷，在线圈人地基） | `automute/engine.{h,cpp}` |
| **主程序：进程级定向静音（P2.4）** | ✅ **真机闭环** | `--proc <PID>` 选目标 App → 引擎 → 声纹判定自动掐声；现为驱动引擎的薄 CLI 前端。隔离原声靠把目标 App 路由到虚拟声卡。真机验证（chrome→CABLE）：放目标A 相似度~0.7→🔇喇叭静音，放异人B ~0.1→🔊放行 | `automute/app_main.cpp` |
| 进程 loopback 抓取（P2.1） | ✅ 真机验证 | 按 PID 抓单个进程音频（MinGW 手补 API）；探针 chrome 实测 -11.8dBFS 正常跳动 | `automute/audio/process_loopback.{h,cpp}` |
| 进程选择枚举（P2.2） | ✅ | `--apps` 扫描【所有活动输出设备】列出会话（PID+进程名+是否出声+输出到哪个设备），路由到虚拟声卡的进程也能找到。P2.3 的 `setProcessMuted` 已移除：真机验证 mute 会同时掐死 loopback 抓取 | `automute/audio/audio_sessions.{h,cpp}` |
| 音频渲染（WASAPI 事件驱动回放） | ✅ | 默认设备，事件驱动低延迟 | `automute/audio/render_playback.cpp` |
| **每应用路由 AppRouter（M4.3）** | 🚧 | 与引擎解耦的独立模块。M4.3a✅ 端点枚举 + VB-CABLE 检测。M4.3b✅ 未公开 `IAudioPolicyConfigFactory`（WinRT 激活，IID 2a59116d 回退、vtable 19 保留+Set/Get/Clear、`\\?\SWD#MMDEVAPI#<id>#{render}` 打包）的 route/restore（eConsole+eMultimedia 双 role）+ RAII 还原 + 崩溃 journal 启动兜底 + ClearAll 逃生舱。**踩坑**：接口类型必须外部链接，否则 GCC -O2 去虚化跳飞。M4.3c✅ 接进 app_main（启动 recoverStaleRoutes→检测→自动路由→退出/Ctrl+C 还原，逐级回退引导手动）。**真机端到端验证**：QQMusic→CABLE→强杀残留→下次启动恢复→journal 清除。⚠️ 已知限制：运行中用户手动改设备会与还原冲突，v1 不做实时设备监听（backlog） | `automute/audio/app_router.{h,cpp}` |
| **GUI 外壳（M4.4）** | 🚧 | C++ webview(WebView2) 宿主 + 纯 HTML/CSS/JS 单页前端，复用引擎+AppRouter；6 个 JS↔C++ bind + 前端 250ms 轮询 getStatus 画仪表。构建/起窗口/prepare 验通，真机交互待验 | `automute/gui_main.cpp`, `automute/gui_html.cpp` |
| 声纹录入 / 存储 | ❌ | M2 | — |
| 特征提取（fbank） | ✅ | dr_wav 读 wav → kaldi-native-fbank 算 80 维 fbank + 均值归一化 | `automute/audio/wav_io.cpp`, `automute/audio/embedder.cpp`, `third_party/dr_libs/`, `third_party/kaldi-native-fbank/` |
| 声纹推理（ONNX Runtime C++） | ✅ | 全管线通：wav→fbank→ECAPA 模型→192 维 embedding（ORT 1.26，MinGW） | `automute/audio/embedder.cpp`, `third_party/onnxruntime/` |
| 声纹提取器 Embedder（可复用） | ✅ | 封装 wav→声纹 + 余弦相似度；验证同人 0.835/异人 0.07 | `automute/audio/embedder.{h,cpp}`, `automute/sim_probe.cpp` |
| 重采样 48k→16k（抗混叠 FIR） | ✅ | Embedder 内置，非 16k 自动降采样；48k 立体声链路验证识别力无损 | `automute/audio/resampler.{h,cpp}` |
| VAD + 阈值调参 | ❌ | M3 | — |

---

*产品形态 = P2 进程 loopback + 虚拟声卡路由隔离原声（P1 设备选择已移除）。P2.4 真机端到端闭环已验证通过。
下一步：M3 调优（压短 1.5s 窗口 / 加 VAD / 调阈值，收窄目标开口头 200~400ms 泄漏）；或产品化 P3+（配置持久化、enroll UX、GUI）。详见 [`exec-plans/productization.md`](exec-plans/productization.md)。*
