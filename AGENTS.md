# AutoMute

> **直播实时定向静音工具**。从一条混合音频里实时掐掉某个指定人的声音，其余原样播放。
> 同时作为深入学 C++ 的载体。
> 本文档是**索引**，细节去 `docs/` 按需读。

---

## 常用文档

| 想了解… | 去这里 |
|---------|--------|
| 顶层设计 / 终点 / 关键决策 / 延迟权衡 | [`docs/DESIGN.md`](docs/DESIGN.md) |
| 功能实现状态（单一事实源） | [`docs/STATUS.md`](docs/STATUS.md) |
| 产品化路线（P1-P5；P1 已移除、P2 已成、P3-5 并入 M4） | [`docs/exec-plans/productization.md`](docs/exec-plans/productization.md) |
| **M4 app 壳 spec（当前里程碑）** | [`docs/exec-plans/m4-app-shell.md`](docs/exec-plans/m4-app-shell.md) |
| M2 声纹识别 exec-plan | [`docs/exec-plans/m2-speaker-id.md`](docs/exec-plans/m2-speaker-id.md) |

---

## 黄金原则

1. **终点即罗盘** — 一切从"最终要做出什么"反推；拿不准的地方先问，不猜
2. **对齐后才动手** — 模糊点清零 + 风险摊开，才开始写代码
3. **C++ 自建有边界** — 实时音频引擎自己写（学习火力），声纹模型借现成（ONNX）
4. **代码变，STATUS 必须同步变**

---

## 进展（速览，细节看 STATUS）

- ✅ M1 音频脊梁 / M2 声纹识别 / P2 进程级定向静音（真机闭环）
- 🅿️ M3 调优暂告段落（滑动窗已接；窗长地板≈1.5s，缩窗反噬）
- ▶️ **当前：M4 app 壳**（GUI 化）。spec 已对齐批准，下一步 M4.2 引擎扩展（多目标 + 最近 N 秒快照）

可执行：`automute_app`(主程序) / `automute_ploop_probe`(抓取诊断) / `automute_sim_probe`(相似度) / `automute_detect`(实时检测)。

---

*Map version: 2026-06-04*
