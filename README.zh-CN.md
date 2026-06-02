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
“现在说话的是不是目标那个人”，是就把这一段掐掉。判断需要一小段音频（~200–400ms，
声学物理下限），因此目标每次开口的**最初一瞬会漏出来**，随后被静音。换取的是
**音画不延迟**——对看口型的内容，这比全程音画不同步更可接受。

设计细节见 [`docs/DESIGN.md`](docs/DESIGN.md)，实现进度见 [`docs/STATUS.md`](docs/STATUS.md)。

## 路线图

| 里程碑 | 内容 | 状态 |
|--------|------|------|
| **M1** | WASAPI loopback 抓取 → 回放，测延迟，手动静音开关 | 🚧 抓取已通 |
| M2 | 录入目标声纹 → ONNX 推理 → 实时比对 → 命中即掐 | ❌ |
| M3 | 调优：压短判断窗口、VAD 抗噪、阈值调参 | ❌ |

## 构建

需要 CMake ≥ 3.20 与一个 C++20 编译器（开发用 MinGW g++ 14）。

```powershell
# 0. 拉取第三方依赖（ONNX Runtime、kaldi-native-fbank、dr_wav、模型）—— 不入库
powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1

# 1. 配置 & 构建
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/automute_probe      # 当前：监听系统输出，打印实时音量条
```

播放任意声音，能看到电平条随之跳动，即说明抓取链路正常。

### 试试声纹识别（可选）

```powershell
# 生成测试语音样本（同人×2 + 异人×1）
python -m pip install datasets soundfile scipy
python scripts\make-test-speakers.py

# 离线：余弦相似度矩阵（同人应高、异人应低）
.\build\bin\automute_sim_probe.exe

# 实时：登记一个目标声音，再播放音频 —— 打印目标是否在说话
.\build\bin\automute_detect.exe models\test_speakers\spkA_1272_1.wav

# 主程序：登记目标 → 从系统输出里自动静音那个人的声音
.\build\bin\automute_app.exe models\test_speakers\spkA_1272_1.wav
```

> ⚠️ 抓取和回放同一个默认设备会产生回声反馈 —— 真正使用时应把来源 App 的音频
> 路由到虚拟声卡（见 `docs/DESIGN.md`）。

## 技术栈

- **C++20** — 实时音频引擎自写（环形缓冲 / 无锁队列 / 低延迟线程）
- **WASAPI** — Windows 系统音频 loopback 抓取与回放
- **ONNX Runtime**（计划中）— 调用预训练轻量声纹模型（ECAPA / x-vector）

## License

[MIT](LICENSE)
