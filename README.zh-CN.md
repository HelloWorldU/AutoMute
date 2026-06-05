<div align="center">

# AutoMute

### 直播实时定向静音

**看直播时，从一条混合音频里实时掐掉某一个指定人的声音，其余照常播放。**

一个深入学习 C++ 的实践项目：实时音频、WASAPI、无锁管线、ONNX 推理。

[![License](https://img.shields.io/github/license/HelloWorldU/AutoMute)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-blue)](https://github.com/HelloWorldU/AutoMute)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Stars](https://img.shields.io/github/stars/HelloWorldU/AutoMute?style=flat)](https://github.com/HelloWorldU/AutoMute/stargazers)

**[🏗 设计](docs/DESIGN.md)** · **[📊 状态](docs/STATUS.md)** · **[English](README.md)**

</div>

---

> 🚧 **开发中。** 同时作为学 C++ 的练兵场——价值既在做出来的工具，也在*它是怎么搭起来的*
> （从零自写的实时音频引擎）。

聚焦场景：**看口型的内容**（有人脸说话）、多人**轮流说话**（不重叠）、**Windows** 平台。

## 它是怎么工作的

你没法掐掉一个还没听到的声音——所以工具抓取系统正在播放的混合音频，实时判断
“现在说话的是不是目标那个人”，是就把这一段掐掉。判断需要一小段音频（实测 ~1.5s，
是本声纹模型的声学下限，见 M3.4），因此目标每次开口的**最初一瞬会漏出来**，随后被
静音。换取的是**音画不延迟**——对看口型的内容，这比全程音画不同步更可接受。

设计细节见 [`docs/DESIGN.md`](docs/DESIGN.md)，实现进度见 [`docs/STATUS.md`](docs/STATUS.md)。

## 路线图

| 里程碑 | 内容 | 状态 |
|--------|------|------|
| **M1** | WASAPI loopback 抓取 → 回放，测延迟，手动静音开关 | ✅ 闭环跑通，稳态 ~20ms |
| **M2** | 录入目标声纹 → ONNX 推理 → 实时比对 → 命中即掐 | ✅ 真机自动掐声验证 |
| **M3** | 调优：压短判断窗口、VAD、阈值 | 🅿️ 暂停 —— 窗长地板 ~1.5s（缩窗反噬，M3.4） |
| **M4** | app 壳：解耦 `AutoMuteEngine` + 自动设备路由 + GUI | ✅ v1 单人闭环真机验通（GUI：选 App→自动路由→在线抓取命名→掐声）。持久化 → v1.1 |

状态单一事实源：[`docs/STATUS.md`](docs/STATUS.md)。

## 构建

需要 CMake ≥ 3.20 与一个 C++20 编译器（开发用 MinGW g++ 14）。

**需自己装一次**（脚本拉不了、app 也装不了）：**[VB-CABLE](https://vb-audio.com/Cable/)**
（虚拟声卡，用于隔离原声——app 会自动路由到它，没装会引导你）和
**[WebView2 运行时](https://developer.microsoft.com/microsoft-edge/webview2/)**
（GUI 需要，Win10/11 多已预装）。

```powershell
# 0. 拉取第三方依赖（不入库 —— third_party 被 gitignore）
powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1

# 1. 配置 & 构建
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. 跑 GUI（在项目根目录跑，models/ 才解析得到）
./build/bin/automute_gui        # 选 App → 抓取说话人 → 切静音开关
```

### CLI / 诊断（可选）

上面的 GUI 是主入口。同一套引擎也有薄 CLI 驱动，外加几个离线探针，调参/排查用：

```powershell
# 生成测试语音样本（同人×2 + 异人×1）
python -m pip install datasets soundfile scipy
python scripts\make-test-speakers.py

# 离线：余弦相似度矩阵（同人应高、异人应低）
.\build\bin\automute_sim_probe.exe

# 实时：登记一个目标声音，再播放音频 —— 打印目标是否在说话
.\build\bin\automute_detect.exe models\test_speakers\spkA_1272_1.wav

# CLI 主程序：列出在出声的进程，再按 PID 指定目标（自动路由到 VB-CABLE）
.\build\bin\automute_app.exe --apps
.\build\bin\automute_app.exe --proc <PID> models\test_speakers\spkA_1272_1.wav
```

> GUI 和 CLI 主程序都会把目标 App 自动路由到 VB-CABLE 隔离原声，再把门控后的音频
> 渲染到你的喇叭——不会回声反馈。离线探针（`sim_probe`/`detect`）直接用默认设备。
> 详见 [`docs/DESIGN.md`](docs/DESIGN.md)。

## 技术栈

- **C++20** — 实时音频引擎自写（环形缓冲 / 无锁队列 / 低延迟线程）
- **WASAPI** — Windows 系统音频 loopback 抓取、进程级 loopback、回放
- **ONNX Runtime** — 预训练轻量声纹模型（wespeaker ECAPA-TDNN，VoxCeleb）
- **WebView2** 经 [webview/webview](https://github.com/webview/webview) — GUI 外壳（C++ 后端 + HTML/CSS/JS 前端）
- 未公开 **`IAudioPolicyConfig`** COM — 每应用输出路由（自动路由到 VB-CABLE，退出还原）

## License

[MIT](LICENSE)
