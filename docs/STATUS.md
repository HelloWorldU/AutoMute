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
| M2 | 接入声纹识别：录入目标样本 → ONNX 推理 embedding → 实时比对 → 命中即掐 | ❌ 拆分见下，详见 [`exec-plans/m2-speaker-id.md`](exec-plans/m2-speaker-id.md) |
| M3 | 调优：压短判断窗口、VAD 抗噪、阈值调参，让开头泄漏收窄 | ❌ |

## M2 子步骤

| # | 目标 | 状态 |
|---|------|------|
| M2.1 | 集成 ONNX Runtime（预编译二进制）+ 跑通 dummy 推理，证明能链接能跑 | ✅ MinGW 直链 MSVC .lib 成功，Env 初始化通过（v1.26.0） |
| M2.2a | 加载真实模型 + 假数据跑通推理，确认 I/O 规格 | ✅ feats[B,T,80] → embs[B,192]，假输入推理成功 |
| M2.2b | 写 fbank 特征提取，喂真 wav → 得到真 embedding | ✅ wav→fbank(298×80)→模型→192维声纹，全管线通 |
| M2.3 | 用真实语音验证可区分性：算多段 embedding，余弦相似度同人>异人；顺带把"算 embedding"封装成可复用函数（即录入核心）| ❌ ← 下一步 |
| M2.4 | 实时分析线程：抓取端分流音频到第二个环形缓冲 → 攒窗口 → 推理 → 余弦比对目标声纹 → 出判定 | ❌ |
| M2.5 | 接线：判定结果驱动 gate（替换手动 m 键，实现自动掐声） | ❌ |

## 核心模块

| 模块 | 状态 | 说明 | 关键文件 |
|------|------|------|----------|
| 音频抓取（WASAPI loopback） | ✅ | 已验证：电平条随系统声音实时跳动，48kHz/2ch float | `automute/audio/loopback_capture.cpp` |
| 实时音频管线（环形缓冲 / 无锁队列 / 低延迟线程） | ✅ | 无锁 SPSC 环形缓冲 + 抓取/渲染双线程；闭环稳态 ~20ms | `automute/audio/ring_buffer.h`, `automute/audio/render_playback.cpp`, `automute/passthrough_main.cpp` |
| 静音门控（gate） | ✅ | 手动开关（m 键），抓取端写静音；M2 接声纹自动触发 | `automute/passthrough_main.cpp` |
| 音频渲染（WASAPI 事件驱动回放） | ✅ | 默认设备，事件驱动低延迟 | `automute/audio/render_playback.cpp` |
| 声纹录入 / 存储 | ❌ | M2 | — |
| 特征提取（fbank） | ✅ | dr_wav 读 wav → kaldi-native-fbank 算 80 维 fbank + 均值归一化 | `automute/audio/wav_io.cpp`, `automute/feat_probe.cpp`, `third_party/dr_libs/`, `third_party/kaldi-native-fbank/` |
| 声纹推理（ONNX Runtime C++） | ✅ | 全管线通：wav→fbank→ECAPA 模型→192 维 embedding（ORT 1.26，MinGW） | `automute/feat_probe.cpp`, `third_party/onnxruntime/` |
| VAD + 阈值调参 | ❌ | M3 | — |

---

*下一步：M2.3 — 拿 2~3 段真实语音（同人×2、异人×1），各算 embedding，算余弦相似度，验证"同人高、异人低"。这是整套方案能否成立的关键证明；顺带把"wav→embedding"封装成可复用函数（= 录入的核心）。*
