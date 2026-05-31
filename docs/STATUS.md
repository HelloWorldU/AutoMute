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
| M2 | 接入声纹识别：录入目标样本 → ONNX 推理 embedding → 实时比对 → 命中即掐 | ❌ |
| M3 | 调优：压短判断窗口、VAD 抗噪、阈值调参，让开头泄漏收窄 | ❌ |

## 核心模块

| 模块 | 状态 | 说明 | 关键文件 |
|------|------|------|----------|
| 音频抓取（WASAPI loopback） | ✅ | 已验证：电平条随系统声音实时跳动，48kHz/2ch float | `automute/audio/loopback_capture.cpp` |
| 实时音频管线（环形缓冲 / 无锁队列 / 低延迟线程） | ✅ | 无锁 SPSC 环形缓冲 + 抓取/渲染双线程；闭环稳态 ~20ms | `automute/audio/ring_buffer.h`, `automute/audio/render_playback.cpp`, `automute/passthrough_main.cpp` |
| 静音门控（gate） | ✅ | 手动开关（m 键），抓取端写静音；M2 接声纹自动触发 | `automute/passthrough_main.cpp` |
| 音频渲染（WASAPI 事件驱动回放） | ✅ | 默认设备，事件驱动低延迟 | `automute/audio/render_playback.cpp` |
| 声纹录入 / 存储 | ❌ | M2 | — |
| 声纹推理（ONNX Runtime C++） | ❌ | M2，借预训练模型 | — |
| VAD + 阈值调参 | ❌ | M3 | — |

---

*下一步：M1 — 先证明"低延迟抓取+处理+回放"这条管线成立，再往上挂识别。*
