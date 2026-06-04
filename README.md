<div align="center">

# AutoMute

### Real-time selective voice muting for livestreams

**Mute one specific person's voice in real time — from a single mixed audio stream, while everyone else keeps playing normally.**

A hands-on project for learning C++ in depth: real-time audio, WASAPI, lock-free pipelines, ONNX inference.

[![License](https://img.shields.io/github/license/HelloWorldU/AutoMute)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-blue)](https://github.com/HelloWorldU/AutoMute)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](CMakeLists.txt)
[![Stars](https://img.shields.io/github/stars/HelloWorldU/AutoMute?style=flat)](https://github.com/HelloWorldU/AutoMute/stargazers)

**[🏗 Design](docs/DESIGN.md)** · **[📊 Status](docs/STATUS.md)** · **[简体中文](README.zh-CN.md)**

</div>

---

> 🚧 **Work in progress.** Also a proving ground for learning C++ — the value is
> as much in *how it's built* (real-time audio engine from scratch) as in the
> finished tool.

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

**Runtime prerequisites** (install once — not fetched by the script):

- **[VB-CABLE](https://vb-audio.com/Cable/)** virtual audio device — the app routes
  the target app's output here to isolate the original sound (auto-routes, restores
  on exit). Without it the muting can't work; if auto-routing is unavailable the app
  guides you to set it manually.
- **[WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/)** —
  for the GUI. Preinstalled on most Windows 10/11; install if the window stays blank.

```powershell
# 0. Fetch source-code deps — ONNX Runtime, kaldi-native-fbank, dr_wav, the model,
#    plus webview + WebView2 SDK headers (GUI). NOT committed (third_party is gitignored).
powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1

# 1. Configure & build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. Run the GUI (from the project root, so models/ resolves)
./build/bin/automute_gui        # pick an app → capture a speaker → toggle mute
```

### Try speaker ID (optional)

```powershell
# Regenerate test speech samples (same speaker ×2 + different speaker ×1)
python -m pip install datasets soundfile scipy
python scripts\make-test-speakers.py

# Offline: cosine-similarity matrix (same speaker should score high, different low)
.\build\bin\automute_sim_probe.exe

# Live: enroll a target voice, then play audio — prints whether the target is speaking
.\build\bin\automute_detect.exe models\test_speakers\spkA_1272_1.wav

# THE APP: enroll a target → it auto-mutes that voice from the system output
.\build\bin\automute_app.exe models\test_speakers\spkA_1272_1.wav
```

> ⚠️ Capturing and rendering the same default device causes echo feedback — for a
> real setup, route the source app's audio through a virtual cable (see `docs/DESIGN.md`).

## Tech stack

- **C++20** — real-time audio engine written from scratch (ring buffer / lock-free queue / low-latency threads)
- **WASAPI** — Windows system-audio loopback capture and playback
- **ONNX Runtime** (planned) — run a pretrained lightweight speaker model (ECAPA / x-vector)

## License

[MIT](LICENSE)
