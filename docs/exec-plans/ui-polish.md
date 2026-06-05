# UI 打磨 exec-plan —— 从"能用骨架"到 Vue 现代界面

> v1 功能真机闭环后，把前端从手写 vanilla 迁到框架并美化。**不动功能逻辑/引擎/声学**；
> 只换呈现层 + 一块独立的"无边框窗口外壳"（要动一点 C++）。状态以 [`../STATUS.md`](../STATUS.md) 为准。

## 已钉决策

- **栈**：Vue 3 + Naive UI + TypeScript + Vite，`vite-plugin-singlefile` 构建成**单文件 HTML**。
- **视觉**：精致暗色 · 现代极简（类 Linear/Vercel）。Naive 暗色主题 + 项目设计令牌。
- **嵌入**：发布**嵌入** `kIndexHtml`（保单 exe）；开发可读 `dist/index.html`（热更）。
- **生成步骤**：`dist/index.html → kIndexHtml` 用**独立脚本** [`scripts/embed-frontend.mjs`](../../scripts/embed-frontend.mjs)（隔离，不塞 CMake）。
- **窗口外壳**：无边框 + 自绘标题栏（对"C++ 零改动"的明确例外）。

## 集成架构（C++/绑定零改动）

```
frontend/ (Vue+TS+Vite) ── npm run build ──► dist/index.html（CSS/JS 全内联单文件）
   └─ node scripts/embed-frontend.mjs ──► 生成 automute/gui_html.cpp 的 kIndexHtml
        └─ C++ set_html(kIndexHtml) 不变；w.bind(...) 9 个绑定契约不变
```

- 前端照样 `await window.startEngine(pid)` 等；`src/webview.d.ts` 给 9 个绑定加 TS 类型。
- 单文件内联 → 仍是"一个 exe 自带界面"。代价：开发多一段 `npm run build`；`node_modules`/`dist` gitignore；`gui_html.cpp` 成生成物。
- **改 UI 闭环**：改 `frontend/src/*.vue` → `npm run build` → `node scripts/embed-frontend.mjs` → 重编 `automute_gui`。

## 落地子步（全 ✅，真机交互层面待用户验）

| # | 内容 |
|---|------|
| **U0 管线** | scaffold `frontend/`；`webview.d.ts` 声明 9 绑定；embed 脚本；"hello Vue" 跑进 webview |
| **U1 复刻功能** | `App.vue` 用 Naive 组件重建全部功能（选 App 按 PID 去重 / 抓取命名 / 目标名单含改名删除 / 仪表 / banner / 自适应轮询）；修横向溢出（`min-width:0`）+ 窗口最小尺寸 |
| **Uw 无边框** | `SetWindowSubclass` 子类化：`WM_NCCALCSIZE` 吃非客户区（最大化留边防盖任务栏）、`WM_NCHITTEST` 缩放热区；保留 WS_OVERLAPPEDWINDOW（Snap/动画/任务栏正常）+ DWM 投影 + Win11 圆角。绑定 `winMinimize/winToggleMaximize/winClose/winDrag/winResize/winIsMaximized`；前端自绘标题栏（Segoe Fluent 图标）+ HTML 边缘缩放手柄 |
| **U2 设计语言** | Naive `themeOverrides` 设计令牌（冷调色板/indigo 主色/统一圆角字号/卡片标题次级化）；品牌渐变点、hover、细滚动条 |
| **U4 锦上添花** | 自定义相似度仪表（渐变 + 50% 阈值刻度 + 超阈红光）；行进出 transition；SVG 图标；静音脉冲 |
| U3 状态细节 | 暂缓（空/错/加载/focus 基础已随 U1/U2 覆盖） |

## 踩坑（UI 侧，避雷；其余通用坑见 STATUS）

- **WebView2 盖满窗口** → 无边框拖拽/缩放父窗口边缘 hit-test 收不到，须 HTML 手柄调 JS→C++ 发 `WM_NCLBUTTONDOWN`（拖 HTCAPTION / 缩放 HTLEFT…）。WebView2 默认不支持 `app-region:drag`。
- 子类化**务必 `DefSubclassProc` 转回 webview 原 proc**，别吞它的消息。
- `w.window()`（webview 0.12）返回 `result<void*>`，取 `.value()`。
- 命名撞坑：JS 函数名 vs 绑定名（`capture`）、绑定名 vs 浏览器内置（`stop`）；`setInterval` 轮询雪崩 —— 详见 STATUS「踩坑」。

## 剩余 / backlog

- U3 状态细节深化；option 文案收敛（设备全名默认隐藏/hover 再显示）；明暗主题切换。
