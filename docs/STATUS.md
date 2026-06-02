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
| M2 | 接入声纹识别：录入目标样本 → ONNX 推理 embedding → 实时比对 → 命中即掐 | ✅ 全子步骤完成，核心闭环建成（自动掐声待真机验），详见 [`exec-plans/m2-speaker-id.md`](exec-plans/m2-speaker-id.md) |
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
| 实时音频管线（环形缓冲 / 无锁队列 / 低延迟线程） | ✅ | 无锁 SPSC 环形缓冲 + 抓取/渲染双线程；闭环稳态 ~20ms | `automute/audio/ring_buffer.h`, `automute/audio/render_playback.cpp`, `automute/passthrough_main.cpp` |
| 静音门控（gate） | ✅ | 手动开关（m 键），抓取端写静音；M2.5 接声纹判定自动触发 | `automute/passthrough_main.cpp` |
| 实时声纹检测 | ✅ | 抓取+分析双线程，1.5s 窗 enroll 比对，打印是否目标在说话 | `automute/detect_main.cpp` |
| **主程序：自动定向静音** | ✅ | 抓取+渲染(带gate)+分析三线程，检测目标→自动静音；核心闭环 | `automute/app_main.cpp` |
| 音频渲染（WASAPI 事件驱动回放） | ✅ | 默认设备，事件驱动低延迟 | `automute/audio/render_playback.cpp` |
| 声纹录入 / 存储 | ❌ | M2 | — |
| 特征提取（fbank） | ✅ | dr_wav 读 wav → kaldi-native-fbank 算 80 维 fbank + 均值归一化 | `automute/audio/wav_io.cpp`, `automute/feat_probe.cpp`, `third_party/dr_libs/`, `third_party/kaldi-native-fbank/` |
| 声纹推理（ONNX Runtime C++） | ✅ | 全管线通：wav→fbank→ECAPA 模型→192 维 embedding（ORT 1.26，MinGW） | `automute/audio/embedder.cpp`, `third_party/onnxruntime/` |
| 声纹提取器 Embedder（可复用） | ✅ | 封装 wav→声纹 + 余弦相似度；验证同人 0.835/异人 0.07 | `automute/audio/embedder.{h,cpp}`, `automute/sim_probe.cpp` |
| 重采样 48k→16k（抗混叠 FIR） | ✅ | Embedder 内置，非 16k 自动降采样；48k 立体声链路验证识别力无损 | `automute/audio/resampler.{h,cpp}` |
| VAD + 阈值调参 | ❌ | M3 | — |

---

*下一步：M3 — 调优。核心闭环已成（automute_app）。待办：① 真机验证自动掐声端到端；② 压短判断窗口(1.5s→更短)减少开头泄漏；③ 滑动窗+VAD 让判定更跟手；④ 阈值调参。另：真正落地需虚拟声卡路由（避免同设备 loopback 回声），见 DESIGN。*
