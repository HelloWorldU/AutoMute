<div align="center">

# AutoMute

### Real-time selective voice muting for livestreams

**Mute one specific person's voice in real time — from a single mixed audio stream, while everyone else keeps playing normally.**

A hands-on project for learning C++ in depth: real-time audio, WASAPI, lock-free pipelines, ONNX inference.

[![CI](https://github.com/HelloWorldU/AutoMute/actions/workflows/ci.yml/badge.svg)](https://github.com/HelloWorldU/AutoMute/actions/workflows/ci.yml)
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
audio (~1.5 s in practice — an acoustic floor for this speaker-ID model, found
in M3.4), so the **first moment of each of the target's turns leaks through**
before muting catches up. What you get in return is **no audio/video delay** —
for lip-sync content, that's more acceptable than a constant A/V mismatch.

Design details in [`docs/DESIGN.md`](docs/DESIGN.md); implementation status in
[`docs/STATUS.md`](docs/STATUS.md).

## Roadmap

| Milestone | Scope | Status |
|-----------|-------|--------|
| **M1** | WASAPI loopback capture → playback, latency, manual mute toggle | ✅ closed loop, steady ~20 ms |
| **M2** | Enroll target voiceprint → ONNX inference → real-time match → gate on hit | ✅ real-machine auto-mute verified |
| **M3** | Tuning: shrink the decision window, VAD, threshold | 🅿️ paused — ~1.5 s window floor (shrinking backfires, M3.4) |
| **M4** | App shell: decoupled `AutoMuteEngine` + auto device-routing + GUI | ✅ v1 single-person loop verified end-to-end (GUI: pick app → auto-route → live-capture & name → mute). Persistence → v1.1 |

Single source of truth for status: [`docs/STATUS.md`](docs/STATUS.md).

## Build

Requires CMake ≥ 3.20 and a C++20 compiler (developed with MinGW g++ 14).

**Install once yourself** (the script can't fetch these, the app can't install them):
**[VB-CABLE](https://vb-audio.com/Cable/)** (virtual audio device, to isolate the
original sound — the app auto-routes to it and guides you if it's missing) and the
**[WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/)** (for
the GUI; usually preinstalled on Windows 10/11).

```powershell
# 0. Fetch third-party deps (not committed — third_party is gitignored)
powershell -ExecutionPolicy Bypass -File scripts\fetch-deps.ps1

# 1. Configure & build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 2. Run the GUI (from the project root, so models/ resolves)
./build/bin/automute_gui        # pick an app → capture a speaker → toggle mute
```

### CLI / diagnostics (optional)

The GUI above is the main entry. The same engine is also driven by a thin CLI,
plus offline probes — handy for tuning and troubleshooting:

```powershell
# Regenerate test speech samples (same speaker ×2 + different speaker ×1)
python -m pip install datasets soundfile scipy
python scripts\make-test-speakers.py

# Offline: cosine-similarity matrix (same speaker should score high, different low)
.\build\bin\automute_sim_probe.exe

# Live: enroll a target voice, then play audio — prints whether the target is speaking
.\build\bin\automute_detect.exe models\test_speakers\spkA_1272_1.wav

# CLI app: list sounding processes, then target one by PID (auto-routes to VB-CABLE)
.\build\bin\automute_app.exe --apps
.\build\bin\automute_app.exe --proc <PID> models\test_speakers\spkA_1272_1.wav
```

> The GUI and CLI app auto-route the target app to VB-CABLE to isolate its
> original sound, then render the gated audio to your speakers — no echo
> feedback. The offline probes (`sim_probe`/`detect`) use the default device
> directly. See [`docs/DESIGN.md`](docs/DESIGN.md).

## Tech stack

- **C++20** — real-time audio engine written from scratch (ring buffer / lock-free queue / low-latency threads)
- **WASAPI** — Windows system-audio loopback capture, per-process loopback, playback
- **ONNX Runtime** — pretrained lightweight speaker model (wespeaker ECAPA-TDNN, VoxCeleb)
- **WebView2** via [webview/webview](https://github.com/webview/webview) — GUI shell (C++ backend + **Vue 3 + Naive UI** front-end, built to a single embedded HTML; frameless custom title bar)
- Undocumented **`IAudioPolicyConfig`** COM — per-app output routing (auto-route to VB-CABLE, restore on exit)

## License

[MIT](LICENSE)
