# Stereix Engine — Minimal C++ Direct3D 11 Integration Sample

This sample demonstrates how to integrate `stereix_sdk.dll` directly into custom C++ game engines, proprietary graphics renderers, and simulation frameworks without Unreal Engine or Unity.

---

## Architecture & Integration Flow

The integration follows a strict, deterministic 5-step lifecycle:

```
[1] Stereix_GetGraphicsRequirements(&reqs)
         │  (Identifies required adapter LUID and per-eye target resolution)
         ▼
[2] D3D11CreateDevice(...) + CreateTexture2D(...)
         │  (Allocates device matching adapter LUID & render target dimensions)
         ▼
[3] Stereix_InitializeDX11(&initInfo, &session)
         │  (Establishes OpenXR session and allocates VR compositor swapchains)
         ▼
[4] Frame Loop:
         ├── Stereix_WaitFrame(session, &frameState)
         ├── Render scene with predicted eye poses (views[0], views[1])
         ├── Stereix_SubmitFrameDX11(session, frameState.token, renderTarget)
         └── Stereix_SyncInput(session) + Stereix_GetControllerState(...)
         │
         ▼
[5] Stereix_Shutdown(session)
            (Destroys VR session and frees all swapchains cleanly)
```

---

## Prerequisites

* Windows 10/11 64-bit
* Visual Studio 2022 (MSVC v143+) with C++ Desktop Development workload
* CMake 3.25+
* An OpenXR-compliant VR runtime (Meta Quest Link, Valve SteamVR, Virtual Desktop, or Pico VR).
  * *Note: If run without an active HMD/runtime, the sample detects this, outputs diagnostic advice, and exits with code 0.*

---

## Building and Running

### Building as Part of Stereix Engine
From the repository root:

```powershell
cmake -B build -S . -A x64 -DBUILD_TESTS=ON
cmake --build build --config Release --target stereix_sample_d3d11
```

### Running the Sample
```powershell
# Run 60 frames (default):
.\build\bin\stereix_sample_d3d11.exe

# Custom frame count:
.\build\bin\stereix_sample_d3d11.exe --frames 120
```

---

## Commercial Compliance & Anti-Cheat

Binaries linking against `stereix_sdk.dll` operate purely in-process via official DirectX 11/12 APIs:
* **Zero Process Injection**: No memory hooking, proxy DLLs, or `CreateRemoteThread`.
* **Anti-Cheat Safe**: Fully compatible with Easy Anti-Cheat (EAC), BattlEye, and Ricochet.
* **Zero-Crash Hardening**: All internal SDK entry points are wrapped with Structured Exception Handling (SEH) so invalid pointers or runtime disconnects return error codes rather than crashing the host process.
