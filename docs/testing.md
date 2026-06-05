# 测试策略与说明

> 测试**代码**在根目录 [`tests/`](../tests/)；本文是它的策略与索引。
> 框架 **doctest**（单头，`third_party/doctest/`，fetch-deps 第 6 步拉），`ctest` 跑。

## 思路：测能划算自动化的，不测要真机的

这个项目的大头风险在硬件/OS 集成（WASAPI、未公开 COM、WebView2、ONNX）——那些**天然要真机 + 真声音 + 人眼/耳验**，自动化又重又脆。所以：

- **自动化**：纯逻辑正确性（环形缓冲/快照/重采样/余弦）、声纹管线离线确定性、构建/启动不崩。
- **人工**：真机掐声、路由真改设备、拖拽/缩放——见下「人工清单」。
- 前端（Vue）逻辑小、价值低，暂不上 Vitest。

## 分层（[`tests/`](../tests/)）

| 层 | 在哪 | 测什么 |
|---|------|--------|
| **Tier 1 单元** `automute_unit_tests` | `tests/unit/` | `SpscRingBuffer`（回绕/写满/空读）、`MonoHistoryRing`（取最近 N 的回绕，对参考实现）、`resampleTo16k`（长度比例/DC 保持/无 NaN）。纯逻辑、无 ONNX、毫秒级 |
| **Tier 2 集成** `automute_embedder_tests` | `tests/integration/` | 声纹管线 wav→fbank→ONNX→embedding：`cosineSimilarity` 性质 + 样本断言「同人 >0.6（实测 0.83）/ 异人 <0.35（实测 0.03）/ 区分度 >0.3」。链 ONNX/kaldi，离线确定可复现 |
| **Tier 3 冒烟** `tests/smoke/smoke.ps1` | `tests/smoke/` | 构建全目标 + ctest + `automute_app --apps` 退出码 + `automute_gui` 起窗口存活。防"改崩构建/启动" |

## 怎么跑

```powershell
# 前置：fetch-deps 已拉 doctest + 模型 + 样本；已 cmake 配置
cmake --build build
ctest --test-dir build --output-on-failure        # Tier 1 + 2

powershell -ExecutionPolicy Bypass -File tests\smoke\smoke.ps1   # Tier 3（含上面两层）
```

测试夹具：声纹样本 wav 在 `tests/data/`（spkA 同人 ×2 + spkB 异人，已入库，小）；模型 24MB 太大走 fetch-deps（`models/`）。路径经编译期宏 `AUTOMUTE_ROOT` 注入，与 cwd 无关。

## CI（GitHub Actions）

[`.github/workflows/ci.yml`](../.github/workflows/ci.yml)：每次 push/PR 到 main，在干净的
`windows-latest` 上 MSYS2+MinGW 拉依赖→编译全部目标→跑 ctest（单元 + 声纹集成）。
**只跑 headless 部分**——GUI 启动/真机掐声需显示器+声卡，CI 跳过、留人工清单。
deps 缓存（key=fetch-deps.ps1 哈希）加速。状态见 README 顶部徽章。

## 人工清单（发版前照点一遍——自动化不划算的真机部分）

跑 `./build/bin/automute_gui`：

1. **选 App → 开始**：提示「已自动路由到 VB-CABLE」；目标 App 直达声消失（改走引擎）。
2. **抓取当前说话人**（某人说话时）→ 命名 → 目标名单出现一行，相似度条随其说话涨高。
3. **掐他开关**：他说话时被静音、相似度条红光过阈；别人照常放行。
4. **改名/删除**：点名字改名、✕ 删目标。
5. **停止**：按钮变「开始」、顶部「未运行」、目标 App 路由还原（音量合成器里变回扬声器）。
6. **无边框窗口**：拖标题栏移动、双击最大化、min/max/close、拖四边四角缩放。
7. **崩溃兜底**：运行中强杀 GUI → 下次启动 `--apps` 看目标 App 路由已自动还原。

## 可加（backlog）

- `AppRouter::packRender`（deviceId 打包）/ journal 读写往返——需暴露内部，价值中等。
- 多目标判定（聚合 max / 任一开静音命中）抽成纯函数后单测。
