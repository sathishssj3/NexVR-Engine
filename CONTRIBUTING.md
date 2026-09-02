# Contributing to NexVR Engine

Welcome! Whether you are adding support for a new game, improving graphics hooks, or fixing a bug, thank you for contributing to **NexVR Engine**.

This guide is designed to get you up, running, and contributing in less than 10 minutes without getting lost in the codebase.

---

## 🚀 5-Minute Developer Quickstart

### Prerequisites
- **OS:** Windows 10 or 11 (64-bit)
- **C++ Compiler:** Visual Studio 2022+ (MSVC v143 with C++20 and Windows 10/11 SDK)
- **Node.js:** v18 or v20 LTS
- **Graphics:** DirectX 11/12 or Vulkan compatible GPU

### 1. Build the C++ Engine
```powershell
# Configure CMake
cmake -B build -S . -A x64

# Compile Release binaries (outputs to build/bin/)
cmake --build build --config Release
```
*Artifacts (`vrinject.dll`, `vr-inject-cli.exe`, shaders, and dependencies) are automatically placed in `build/bin/`.*

### 2. Run C++ Unit Tests
```powershell
ctest --test-dir build -C Release --output-on-failure
```

### 3. Start the Electron/React Launcher
```powershell
cd launcher
npm install
npm run dev
```

### 4. Run Launcher Tests & Typecheck
```powershell
cd launcher
npm run test:ci
```

---

## 🗺️ Codebase Map & Subsystems

```text
┌────────────────────────────────────────────────────────┐
│  Launcher (launcher/)                                  │
│  ├── React UI (launcher/src/)                          │
│  └── Electron Backend (launcher/electron/)             │
└──────────────────────────┬─────────────────────────────┘
                           │ Spawns with Administrator Manifest
┌──────────────────────────▼─────────────────────────────┐
│  CLI Injector (src/injector/main.cpp)                  │
│  └── Injects vrinject.dll via CreateRemoteThread       │
└──────────────────────────┬─────────────────────────────┘
                           │ Target Game Process
┌──────────────────────────▼─────────────────────────────┐
│  Core Engine DLL (vrinject.dll)                        │
│  ├── Core (src/core/): RuntimeState, Config, Engine    │
│  ├── Hooks (src/hooks/): DX11, DX12, Vulkan Present    │
│  ├── Heuristics (src/heuristics/): Matrices & Depth    │
│  ├── Shaders (shaders/): Stereo Reprojection Compute   │
│  └── OpenXR (src/openxr/): Frame submission to HMD     │
└────────────────────────────────────────────────────────┘
```

Detailed documentation for each subsystem:
- [src/core/README.md](src/core/README.md) — State machine, configuration, and frame coordinator.
- [src/hooks/README.md](src/hooks/README.md) — MinHook detours for DX11, DX12, and Vulkan.
- [src/rendering/README.md](src/rendering/README.md) — Stereo compute shaders and OpenXR submitters.
- [src/heuristics/README.md](src/heuristics/README.md) — Memory heuristics and depth ranking.

---

## 🎮 How to Add a New Game Profile in 3 Minutes

You **do not** need to write C++ code to support a new game! All game settings are declarative JSON files in the `profiles/` directory.

### Step 1: Create a profile in `profiles/`
Create a file named `profiles/<appId>_<game_slug>.json` (or `<slug>.json` for custom titles):

```json
{
  "$schema": "./schema.json",
  "id": "990080",
  "name": "My New Game",
  "engine": "UnrealEngine4",
  "api": "DX12",
  "reverseZ": true,
  "rowMajorMatrices": true,
  "motionAimSensitivity": 1.0,
  "useRecommendedResolution": true,
  "srgbCorrection": true,
  "depthSubmission": true,
  "rawInputMode": true,
  "autoInjectOnLaunch": true
}
```

### Supported Fields
- **`engine`**: `"UnrealEngine4"`, `"UnrealEngine5"`, `"Unity"`, or `"Generic"`.
- **`api`**: `"DX11"`, `"DX12"`, or `"Vulkan"`.
- **`reverseZ`**: `true` for UE4/UE5/Unity (modern reversed floating-point depth buffer).
- **`rowMajorMatrices`**: `true` for Unreal Engine, `false` for Unity / DirectX standard.
- **`srgbCorrection`**: `true` if colors look washed out or milky in the headset.

### Step 2: Test your profile
Open the launcher (`npm run dev`) or test via CLI. The launcher automatically picks up your profile!

---

## 🛡️ The 5 Golden Rules of NexVR Engine Development

To keep the engine crash-free, scalable, and maintainable, all contributions must respect these rules:

1. **MinHook Lifecycle Ownership**:
   `HookManager` (`src/core/hook_manager.cpp`) owns `MH_Initialize()` and `MH_Uninitialize()`. Never add MinHook initialization in individual hooks (`BUG-01/02`).
2. **Non-Blocking Render Thread**:
   The game's render thread must remain fluid (< 11.1 ms). Never perform blocking calls (e.g. `xrCreateInstance`, signature scans, file I/O) synchronously inside `Present()` or `FrameCoordinator`.
3. **No Hardcoded Game Name Collisions**:
   Never add `if (exeName.find("MyGame") != std::string::npos)` inside core C++ files (`QUAL-04`). All game-specific behavior belongs in `profiles/*.json`.
4. **Buffer Release on Resize**:
   In DX11 and DX12, all backbuffer references, SRVs, and RTVs must be released prior to calling `ResizeBuffers` to avoid driver device loss (`DXGI_ERROR_INVALID_CALL`).
5. **Anti-Cheat Safety**:
   Never attempt to bypass active anti-cheat systems (Vanguard, EAC, BattlEye). NexVR Engine is strictly for single-player and offline titles.

---

## 🧪 Testing Checklist Before Submitting a PR

1. [ ] C++ unit tests pass:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
2. [ ] Launcher static and E2E regression tests pass:
   ```powershell
   cd launcher
   npm run test:ci
   ```
3. [ ] Code compiles cleanly with MSVC x64 under Release mode.
4. [ ] No hardcoded game paths or temporary files checked in.
