# AutoMute

**English** | [简体中文](README.zh-CN.md)

> Watching a livestream, **mute one specific person's voice in real time, with
> low latency** — from a single mixed audio stream, while everyone else keeps
> playing normally. Also a hands-on project for learning C++ in depth.

Target scenario: **lip-sync-sensitive content** (faces talking on screen),
people speaking **in turns** (non-overlapping), on **Windows**.

## How it works

You can't mute a voice you haven't heard yet. So the tool captures the mixed
audio the system is playing, decides in real time *"is the current speaker the
target?"*, and gates out that span if so. The decision needs a short slice of
audio (~200–400 ms — a physical floor for acoustic speaker ID), so the **first
moment of each of the target's turns leaks through** before muting catches up.
What you get in return is **no audio/video delay** — for lip-sync content,
that's more acceptable than a constant A/V mismatch.

Design details in [`docs/DESIGN.md`](docs/DESIGN.md); implementation status in
[`docs/STATUS.md`](docs/STATUS.md).

## Roadmap

| Milestone | Scope | Status |
|-----------|-------|--------|
| **M1** | WASAPI loopback capture → playback, measure latency, manual mute toggle | 🚧 capture working |
| M2 | Enroll target voiceprint → ONNX inference → real-time match → gate on hit | ❌ |
| M3 | Tuning: shrink the decision window, VAD noise robustness, threshold tuning | ❌ |

## Build

Requires CMake ≥ 3.20 and a C++20 compiler (developed with MinGW g++ 14).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/automute_probe      # current: listens to system output, prints a live level meter
```

Play any sound — the level meter should react, confirming the capture path works.

## Tech stack

- **C++20** — real-time audio engine written from scratch (ring buffer / lock-free queue / low-latency threads)
- **WASAPI** — Windows system-audio loopback capture and playback
- **ONNX Runtime** (planned) — run a pretrained lightweight speaker model (ECAPA / x-vector)

## License

[MIT](LICENSE)
