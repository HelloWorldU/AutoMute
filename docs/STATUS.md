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
| M3 | 调优：压短判断窗口、VAD 抗噪、阈值调参，让开头泄漏收窄 | ❌ ← 下一步 |

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
| **主程序：进程级定向静音（P2.4）** | ✅ **真机闭环** | `--proc <PID>` 选目标 App → 进程 loopback 抓取 → 渲染门控版到默认设备 → 声纹判定自动掐声；进程+渲染+分析三线程。隔离原声靠把目标 App 路由到虚拟声卡。真机验证（chrome→CABLE）：放目标A 相似度~0.7→🔇喇叭静音，放异人B ~0.1→🔊放行 | `automute/app_main.cpp` |
| 进程 loopback 抓取（P2.1） | ✅ 真机验证 | 按 PID 抓单个进程音频（MinGW 手补 API）；探针 chrome 实测 -11.8dBFS 正常跳动 | `automute/audio/process_loopback.{h,cpp}` |
| 进程选择枚举（P2.2） | ✅ | `--apps` 扫描【所有活动输出设备】列出会话（PID+进程名+是否出声+输出到哪个设备），路由到虚拟声卡的进程也能找到。P2.3 的 `setProcessMuted` 已移除：真机验证 mute 会同时掐死 loopback 抓取 | `automute/audio/audio_sessions.{h,cpp}` |
| 音频渲染（WASAPI 事件驱动回放） | ✅ | 默认设备，事件驱动低延迟 | `automute/audio/render_playback.cpp` |
| 声纹录入 / 存储 | ❌ | M2 | — |
| 特征提取（fbank） | ✅ | dr_wav 读 wav → kaldi-native-fbank 算 80 维 fbank + 均值归一化 | `automute/audio/wav_io.cpp`, `automute/audio/embedder.cpp`, `third_party/dr_libs/`, `third_party/kaldi-native-fbank/` |
| 声纹推理（ONNX Runtime C++） | ✅ | 全管线通：wav→fbank→ECAPA 模型→192 维 embedding（ORT 1.26，MinGW） | `automute/audio/embedder.cpp`, `third_party/onnxruntime/` |
| 声纹提取器 Embedder（可复用） | ✅ | 封装 wav→声纹 + 余弦相似度；验证同人 0.835/异人 0.07 | `automute/audio/embedder.{h,cpp}`, `automute/sim_probe.cpp` |
| 重采样 48k→16k（抗混叠 FIR） | ✅ | Embedder 内置，非 16k 自动降采样；48k 立体声链路验证识别力无损 | `automute/audio/resampler.{h,cpp}` |
| VAD + 阈值调参 | ❌ | M3 | — |

---

*产品形态 = P2 进程 loopback + 虚拟声卡路由隔离原声（P1 设备选择已移除）。P2.4 真机端到端闭环已验证通过。
下一步：M3 调优（压短 1.5s 窗口 / 加 VAD / 调阈值，收窄目标开口头 200~400ms 泄漏）；或产品化 P3+（配置持久化、enroll UX、GUI）。详见 [`exec-plans/productization.md`](exec-plans/productization.md)。*
