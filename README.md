# NexVR Engine

<p align="center">
  <img src="assets/logo.png" width="200" alt="NexVR Engine Logo" />
</p>
[![Build](https://github.com/sathishssj3/NexVR-Engine/actions/workflows/release.yml/badge.svg)](https://github.com/sathishssj3/NexVR-Engine/actions/workflows/release.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

> **Universal VR Injector** — Brings native OpenXR stereo rendering to flat-screen PC games by hooking into DX11, DX12, and Vulkan render pipelines.

NexVR Engine intercepts a game's graphics pipeline in real time, converts its mono output into stereoscopic VR frames, and submits them directly to your headset via OpenXR. It ships as an Electron/React launcher with a robust C++ injection engine underneath.

---

## ✨ Key Features

| Feature | Description |
|---------|-------------|
| **Universal Graphics Hook** | Hooks DirectX 11, DirectX 12, and Vulkan swapchains automatically |
| **Stereo Rendering Pipeline** | GPU compute shaders for stereo warp, depth reconstruction, and disocclusion fill |
| **Comfort Guard** | Motion-sickness reduction via vignette and comfort analysis shaders |
| **Asynchronous Spacewarp** | Adaptive frame interpolation to maintain smooth headset framerates |
| **Smart Steam Integration** | Auto-scans local Steam libraries and detects compatible games |
| **Auto Headset Detection** | Detects active OpenXR runtime (Meta Quest, SteamVR, WMR) from the Windows Registry |
| **Live Session Logs** | Streams real-time injection and hooking logs to the launcher UI |
| **Per-Game Profiles** | Save individual VR configs, resolutions, and input settings per game |

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Electron/React Launcher (launcher/)                    │
│  ├── Game Library Browser (Steam auto-detect)           │
│  ├── VR Settings Panel (per-game profiles)              │
│  └── Live Session Log Viewer                            │
└────────────────┬────────────────────────────────────────┘
                 │  spawns vr-inject-cli.exe
┌────────────────▼────────────────────────────────────────┐
│  C++ Injection Engine (src/)                            │
│  ├── DLL Injector (CreateRemoteThread)                  │
│  ├── Graphics Hooks (DX11 / DX12 / Vulkan Present)     │
│  ├── Stereo Pipeline (7 GPU compute shaders)            │
│  ├── OpenXR Manager (headset session + frame submit)    │
│  ├── Motion Predictor (IMU-fused head tracking)         │
│  ├── Comfort Guard (motion-sickness reduction)          │
│  ├── AI Matrix Classifier (view/projection detection)   │
│  └── ImGui Overlay (in-game debug HUD)                  │
└─────────────────────────────────────────────────────────┘
```

---

## 📦 Installation

1. Download the latest **NexVR Engine Setup.exe** from the [Releases](https://github.com/sathishssj3/NexVR-Engine/releases) page.
2. Run the installer — it installs to your local AppData by default.
3. Launch **NexVR Engine** from your Start Menu.

### Requirements

- Windows 10/11 (x64)
- A VR headset with an OpenXR-compatible runtime (Meta Quest Link, SteamVR, or WMR)
- DirectX 11/12 or Vulkan-based game
- Steam (for auto-detection of game libraries)

---

## 🚀 How to Use

1. **Open the Launcher** — Ensure your VR headset is connected and your OpenXR runtime is running. The bottom-left status indicator should glow green.
2. **Select a Game** — Browse your auto-detected Steam library in the sidebar.
3. **Configure Settings** — Adjust VR-specific settings for the selected game.
4. **Inject** — Click the **▶ INJECT** button. The launcher starts the game via Steam, locates its process, and injects the VR DLL.
5. **Put on your Headset** — Once injected, the game submits frames directly to your headset!

### Configuration Options

| Setting | Description |
|---------|-------------|
| **Use Recommended Resolution** | Overrides game render resolution to match headset native panel resolution |
| **Motion Sensitivity** | Adjusts head-tracking and controller movement scaling |
| **Raw Input Mode** | Bypasses Windows input processing for lower latency tracking |
| **sRGB Correction** | Applies gamma correction to prevent washed-out VR colors |
| **Depth Submission** | Submits depth buffer for SpaceWarp and improved reprojection |
| **Auto-Inject on Launch** | Automatically injects when the game executable is detected |

---

## 🛠️ Building from Source

### Prerequisites

- **MSVC** (Visual Studio 2022 with C++ workload)
- **CMake** 3.20+
- **Node.js** 20+
- **Vulkan SDK** (optional, for Vulkan backend)
- **DXC** (DirectX Shader Compiler, downloaded automatically by CMake)

### Build Steps

```bash
# 1. Clone the repository
git clone https://github.com/sathishssj3/NexVR-Engine.git
cd NexVR-Engine

# 2. Build the C++ engine (DLL, CLI, and injector)
cmake -B build -S . -A x64
cmake --build build --config Release

# 3. Build and package the Electron launcher
cd launcher
npm install
npm run pack
```

The NSIS installer will be generated in `launcher/dist-electron/`.

---

## 🔍 Troubleshooting

| Problem | Solution |
|---------|----------|
| **Game crashes on inject** | Disable conflicting overlays (Discord, MSI Afterburner, RivaTuner) |
| **"Game path not found"** | Click **Rescan Library** at the bottom of the sidebar |
| **Headset shows black** | Check Live Session Logs — try setting the game to Windowed/Borderless mode |
| **No headset detected** | Verify your OpenXR runtime in `HKLM\SOFTWARE\Khronos\OpenXR\1` |

---

## ⚠️ Security Notice

> [!WARNING]
> Both `vrinject.dll` and `vr-inject-cli.exe` **must be code-signed** before public distribution. Unsigned injection DLLs are flagged by Windows Defender and antivirus software. Set `SIGN_CERT_PATH` and `SIGN_CERT_PASS` environment variables before building to enable automatic signing.

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).

---

*Developed by sathishssj3*
