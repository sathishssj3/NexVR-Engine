# System Architecture Guide - NexVR Engine

This document is the single source of truth for our technical design, repository structure, and subsystems. Keep this document updated whenever new system-level features are introduced, directory paths change, or new technologies are integrated.

---

## 1. Technical Stack

*   **Engine / Core Injector:** C++, MSVC x64 (VS 2022+)
*   **APIs / Graphics:** Vulkan SDK, DirectML (1.13.1), OpenXR SDK (1.0.34)
*   **Machine Learning / AI:** ONNX Runtime DirectML (1.16.3)
*   **Injection / Hooking:** MinHook
*   **Launcher / UI:** Electron, React, Vite, TypeScript
*   **Testing:** GoogleTest (C++), Playwright & Vitest (Launcher)
*   **Build System:** CMake

---

## 2. Directory & Repository Layout

All team members and AI assistants must strictly follow this folder convention to maintain a tidy repository.

```text
├── .github/
│   └── workflows/          # GitHub Actions for CI/CD & Automated Testing
├── docs/                   # Product requirements, project memory, specs
├── profiles/               # Declarative per-game JSON profiles & schema
├── src/                    # C++ source files for the Universal VR Injector
│   ├── core/               # Runtime state, capability registry, config manager
│   ├── hooks/              # DX11, DX12, Vulkan, and generic API hooks
│   ├── rendering/          # Stereo reprojection shaders and graphics backends
│   ├── heuristics/         # Camera matrix and depth buffer classifiers
│   ├── memory_scanner/     # Dynamic heap and signature scanner
│   ├── ai/                 # AI scheduler and neural inpainter
│   └── injector/           # Main injector executable and DllMain entry points
├── tests/                  # C++ gtest suites
├── launcher/               # Electron & React based GUI for users
├── build/                  # CMake build output directory
│   └── bin/                # Unified output dir (vrinject.dll, CLI, onnxruntime, etc)
│       └── proxy/          # Proxy DLLs (dxgi.dll, d3d11.dll) - isolated to avoid shadowing
├── shaders/                # HLSL shaders compiled at build time
├── CONTRIBUTING.md         # Developer onboarding & quickstart guide
├── README.md               # Product overview and user instructions
└── build.bat               # Automated one-click build script
```

---

## 3. Core System Components & Interactions

```text
[ Electron Launcher ] 
        │
        ▼ (Spawns process)
[ vr-inject-cli.exe ] 
        │
        ▼ (Injects DLL)
[ Target Game Process ] 
        │
        ├─► [ vrinject.dll ] (Initializes RuntimeState & Hooks)
        │       ├─► Intercepts DXGI / D3D11 / Vulkan
        │       └─► Forwards to [ OpenXR SDK ]
```

*   **Launcher:** Provides a user-friendly interface to configure game settings, download AI weights, and trigger the CLI injector.
*   **CLI Injector:** A requireAdministrator manifest executable that handles the actual injection of `vrinject.dll` into the target process.
*   **vrinject.dll:** The core runtime. Initializes via `RuntimeState`, uses `HookManager` to intercept graphics API calls, and bridges game rendering into stereo VR via OpenXR.

---

## 4. Key Subsystems & Design Rules

### 1. HookManager & RuntimeState
*   `HookManager` owns the MinHook lifecycle. Do not add isolated init/shutdown logic inside individual hooks (this avoids known bugs like BUG-01/02).
*   `RuntimeState` orchestrates sibling subsystems (capability registry, async diagnostics). Use `std::atomic` for shared hook globals (DEAD-02/04).

### 2. Graphics Proxies & Shaders
*   Proxy DLLs (`build/bin/proxy/dxgi.dll`, `d3d11.dll`) must deliberately NOT sit in `bin/` alongside the CLI and injector DLL to avoid 0xC000007B crashes caused by shadowing system DLLs.
*   Shaders under `shaders/*.hlsl` are compiled at build time by `fxc`, `dxc`, and `glslc` into headers (`*_cs_dx11.h`), and raw files are also available to the launcher for dynamic compilation.

---

## 5. Security, Anti-Cheat, & Performance Policies

*   **Anti-Cheat Posture:** Never attempt to bypass active anti-cheat. Strict-AC titles (Tier 1/2/3 as defined in project memory) are unsupported. The injector is designed for offline/moddable games.
*   **Code Signing:** Unsigned injection DLLs trigger AV/Defender. Use `SIGN_CERT_PATH`/`SIGN_CERT_PASS` in the build pipeline when preparing releases.
*   **Performance Budget:** Strict 11.1 ms frame limit. Sync AI path on render thread must stay under < 1.5 ms; async AI processing offloaded to worker pool.
