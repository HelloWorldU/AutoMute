# UI 打磨 exec-plan —— 从"能用骨架"到"好看好用"

> v1 功能已真机闭环（见 [`m4-app-shell.md`](m4-app-shell.md)）。当前界面是把链路跑通的
> **骨架**，存在布局 bug + 颜值低。本阶段**纯前端**把它做得耐拉伸、好看、好用。
> **不改功能逻辑、不改 C++ 绑定契约**——只动 [`gui_html.cpp`](../../automute/gui_html.cpp) 的呈现层。
> 实现状态以 [`../STATUS.md`](../STATUS.md) 为准。

## 目标

1. **先修坏**：布局 bug（横向溢出、控件挤压、拉伸错位）。
2. **再变美**：统一的视觉语言（色板/间距/字号/圆角/状态色），按钮/输入/卡片/仪表精致化。
3. **耐用**：窗口任意尺寸下不破版；空/错/加载状态有像样的呈现。

## 现状诊断（具体问题）

| 问题 | 根因 | 证据 |
|------|------|------|
| **横向溢出**：出现横滚动条、「刷新」被挤出右边 | flex 子元素 `select{flex:1}` 缺 `min-width:0`，被长选项（"QQMusic.exe · PID … → 扬声器 (Realtek…)"）撑开不收缩 | 截图底部横滚动条 + 刷新按钮只露一半 |
| **窗口拉伸破版** | `set_size(560,640)` 无最小尺寸；布局靠 flex 但未约束下限 | 缩小窗口控件挤压/溢出 |
| **目标行拥挤** | 一行塞 6 元素（名/条/数值/状态丸/掐他/✕），窄窗下挤 | `.tgt` 布局 |
| **颜值低** | 朴素纯色按钮、间距偏挤、层级弱、仪表条单调、无统一状态色与图标语言 | 整体观感 |
| **信息密度** | option 文本过长（含设备全名）撑宽下拉 | `listApps` 文案 |

## 范围

| | |
|---|---|
| **In** | 新前端工程（Vue+Naive+TS+Vite，构建成单文件嵌入）；前端构建管线 + 文档/gitignore；响应式 + 空/错/加载态；**窗口外壳**（无边框 + 自绘标题栏 → 改 HWND 样式/子类化 WndProc + min/max/close 绑定，**对"C++ 零改动"的明确例外**） |
| **Out** | 现有 9 个 C++ 绑定的**调用契约**不动（只新增 winMinimize/winToggleMaximize/winClose/winDrag）；功能逻辑/引擎/声学不动；多人名单/持久化（v1.1 **功能**线，另算） |

## 决策点（动手前对齐，带推荐）

1. **前端技术栈** ✅ 已钉：**Vue 3 + Naive UI + TypeScript + Vite**，构建成**单文件 HTML**
   （`vite-plugin-singlefile`，CSS/JS 全内联）。理由：CN 生态成熟、组件全、暗色主题现成、
   上手平缓，"好看"门槛低，适合这种小工具。后端 C++ 完全不受影响。

2. **集成架构**（关键，决定 C++ 侧不变）——见下「集成架构」节。框架构建出**单个自包含 HTML**，
   经一个小步骤变成 `kIndexHtml` 字符串（或运行时读取），`set_html` 与 9 个绑定契约**不变**。

3. **视觉方向** ✅ 已钉：**精致暗色 · 现代极简**（类 Linear/Vercel 冷静现代感）。
   以 Naive UI 暗色主题打底，再覆一层项目设计令牌（主色/间距/圆角）。明暗切换暂不做。

4. **窗口行为**——固定起始尺寸 + **设最小尺寸**（防破版），内容响应式。

5. **范围深度**——分步交付：先 U0 把框架管线打通，再 U1 复刻现有功能界面，之后逐步美化。

## 集成架构（框架 ↔ C++/webview，C++ 侧零改动）

```
frontend/                      ← 新前端工程（Vue/React/Svelte + TS + Vite）
  src/  ...组件/状态...          调 window.start()/capture()/getStatus()... （加一份 .d.ts 类型声明）
  npm run build
   └─ vite-plugin-singlefile → dist/index.html   ← 单个自包含 HTML（CSS+JS 全内联）
        └─ 小步骤：dist/index.html → gui_html.cpp 的 kIndexHtml 字符串
             （脚本生成，或 CMake 构建期生成；亦可改为运行时读 dist/index.html）
                └─ C++ set_html(kIndexHtml) 不变；w.bind(...) 9 个绑定契约不变
```

- **绑定不变**：框架里照样 `await window.start(pid)` 等；webview 注入的 `window.xxx` 仍是那套 RPC。
  加一个 `webview.d.ts` 声明 `window.start/capture/...` 的类型，TS 里有提示。
- **保持单 exe**：单文件内联 + 嵌进字符串 → 仍是"一个 exe 自带界面"，不散落资源。
- **代价**：开发流程多一段 `npm install && npm run build`；`fetch-deps`/构建文档要加前端构建阶段；
  `node_modules`/`frontend/dist` 入 gitignore。

## 落地子步（建议）

| # | 目标 | 要点 |
|---|------|------|
| **U0 打通框架管线** ✅ | Vue 工程 → "hello Vue" 跑进 webview | ✅ `frontend/`（Vite6+TS+Vue3+Naive UI）scaffold；`src/webview.d.ts` 声明 9 绑定；`scripts/embed-frontend.mjs` 把 `dist/index.html`(单文件 206KB) → `gui_html.cpp` 的 `kIndexHtml`；重编 `automute_gui` 起窗口存活验通。listApps 按钮待真机点验 |
| **U1 复刻现有功能** ✅ | 用 Vue/Naive 组件重建当前界面 | ✅ `App.vue` 复刻全部：NSelect(选 App,按 PID 去重) / NInput 抓取命名 / 目标名单(NProgress 仪表条+NSwitch 开关+点名改名+✕删) / 聚合仪表 / NAlert banner / 自适应自调度轮询；修布局 bug（`.grow{min-width:0}` 防溢出 + 窗口 `WEBVIEW_HINT_MIN` 最小尺寸）。构建/起窗口验通，真机交互待验 |
| **Uw 窗口外壳** ✅ | 无边框 + 自绘标题栏 | ✅ `SetWindowSubclass` 子类化 webview 窗口过程：`WM_NCCALCSIZE` 吃掉原生标题栏/边框（最大化留边防盖任务栏）、`WM_NCHITTEST` 重建四边四角缩放热区；保留 WS_OVERLAPPEDWINDOW 风格→Snap/动画/任务栏正常；DWM 投影。绑定 `winMinimize/winToggleMaximize/winClose/winDrag/winIsMaximized`。前端自绘标题栏（Segoe Fluent 图标 min/max/close + `winDrag` 拖拽，避开 WebView2 不默认支持 app-region:drag）。构建/起窗口存活验通，真机拖拽/缩放/按钮待验 |
| **U2 设计语言** | 主题与组件精致化 | 暗色主题 + 设计令牌（色板/间距/圆角/阴影/字号）；按钮/输入/卡片/丸子/仪表统一质感 |
| **U3 状态与细节** | 边角不掉链子 | 空名单/未运行/加载/错误态；disabled/hover/focus；窄窗自适应 |
| **U4（可选）** | 锦上添花 | 图标、微动效、相似度仪表更生动的可视化 |

## 约束与 note

- **WebView2 = Chromium 内核**：现代 CSS（flex/grid/CSS 变量/容器查询/`:focus-visible`）都能用，放心。
- 改动只在 `gui_html.cpp` 那段内嵌 HTML 字符串里；窗口尺寸在 `gui_main.cpp`。
- **绝不动** `listApps/start/stop/capture/setMuted/renameTarget/removeTarget/getStatus/cableStatus` 的调用契约（JS 调名 + 返回 JSON 结构）——美化是纯呈现层。
- 验证方式：改完重编 `automute_gui` → 起窗口看布局 + 拉伸 + 各状态；功能回归靠已验的真机流程。

## 窗口外壳：无边框 + 自绘标题栏（已定要做）

去掉 Windows 原生标题栏（最小化/最大化/关闭、系统边框），**自绘一个 app 内的现代标题栏**
（VS Code/Discord 那类）。这块**要动 C++**（原生窗口层，前端碰不到）——是对"换框架 C++ 零改动"
的一个**明确划出的例外**，独立可控。

**做法（Windows + webview）：**
- 拿 `w.window()` 得 HWND → 去掉 `WS_CAPTION`，**保留 `WS_THICKFRAME`+`WS_MINIMIZEBOX`/`MAXIMIZEBOX`**
  （留着才有缩放/任务栏/Aero Snap/双击最大化）。
- 子类化窗口过程（`SetWindowSubclass`，**务必转调 webview 原 proc**，别吞它的消息）：
  - `WM_NCCALCSIZE` 吃掉非客户区 → 客户区铺满整窗（无原生边框）。
  - `WM_NCHITTEST` 重建四边/四角**缩放热区** + 标题栏拖拽区。
- 前端自绘标题栏：app 名 + min/max/close 按钮（Naive UI / 自绘 SVG）。
  - 拖拽优先 CSS `-webkit-app-region: drag`（WebView2/Edge 148 支持）；按钮区 `no-drag`。
    兜底：JS→C++ `winDrag`（`ReleaseCapture` + `WM_NCLBUTTONDOWN/HTCAPTION`）。
- 新增 C++ 绑定：`winMinimize` / `winToggleMaximize` / `winClose`（操作 HWND）。

**风险/note：** 子类化别破坏 webview 消息处理（转调原 proc）；最大化要修"盖住任务栏"；
圆角/阴影自绘；DPI 缩放下热区尺寸。**这块是本阶段最硬的一段，单列子步、先小窗验证。**

待定（U 该步动手前）：缩放/贴边是否保留（推荐**保留**，你抱怨过拉伸）；窗口圆角+投影要不要。

## 待对齐（动手前钉）

- option 文案收敛（设备全名是否默认隐藏、hover/副行再显示）。
- 无边框窗口的圆角/投影、是否保留缩放贴边（倾向保留）——U（窗口外壳）步再定。

## 已钉决策小结

- 栈：**Vue 3 + Naive UI + TS + Vite**，构建单文件 HTML 嵌入。
- 视觉：**精致暗色 · 现代极简**。
- **嵌入方式**：发布**嵌入**进 `kIndexHtml`（保单 exe）；开发可**直接读 `dist/index.html`**（热更顺）——两便。
- **生成步骤**：`dist/index.html → kIndexHtml` 用**独立脚本**（隔离、好维护），不塞进 CMake。
- **窗口外壳**：无边框 + 自绘标题栏（要动 C++，独立例外）。
- 节奏：U0 框架管线 → U1 复刻功能 → **Uw 窗口外壳（无边框）** → U2 设计语言 → U3 状态细节 →（U4 可选）。
