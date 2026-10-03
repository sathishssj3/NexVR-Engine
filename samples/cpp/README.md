# Stereix Engine — Minimal C++ Integration Samples (Direct3D 11 & Direct3D 12)

These samples demonstrate how to integrate `stereix_sdk.dll` directly into custom C++ game engines, proprietary graphics renderers, and enterprise simulation frameworks without Unreal Engine or Unity.

---

## Architecture & Integration Flow

The integration follows a deterministic 5-step lifecycle across both DirectX 11 and DirectX 12:

```text
[1] Query Requirements:
    - Stereix_GetGraphicsRequirements(&reqs)         [D3D11]
    - Stereix_GetGraphicsRequirementsDX12(&reqs)     [D3D12]
         │  (Identifies required adapter LUID and per-eye target resolution)
         ▼
[2] Allocate Device & Render Target:
    - D3D11CreateDevice(...) + CreateTexture2D(...)
    - D3D12CreateDevice(...) + CreateCommandQueue(...) + CreateCommittedResource(...)
         │  (Allocates matching GPU resources on the runtime-specified adapter)
         ▼
[3] Initialize Session:
    - Stereix_InitializeDX11(&initInfo, &session)
    - Stereix_InitializeDX12(&initInfo, &session)
         │  (Establishes OpenXR session and allocates VR compositor swapchains)
         ▼
[4] Frame Loop:
    ├── Stereix_WaitFrame(session, &frameState)
    ├── Render scene with predicted eye poses (views[0], views[1])
    ├── Submit Frame:
    │     - Stereix_SubmitFrameDX11(session, frameState.token, renderTarget)
    │     - Stereix_SubmitFrameDX12(session, frameState.token, renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET)
    └── Stereix_SyncInput(session) + Stereix_GetControllerState(...)
         │
         ▼
[5] Clean Teardown:
    Stereix_Shutdown(session)
        (Destroys VR session and frees all compositor swapchains cleanly)
```

---

## Prerequisites

* Windows 10/11 64-bit
* Visual Studio 2022 (MSVC v143+) with C++ Desktop Development workload
* CMake 3.25+
* An OpenXR-compliant VR runtime (Meta Quest Link, Valve SteamVR, Virtual Desktop, or Pico VR).
  * *Note: If executed in headless CI or non-VR developer machines, the samples detect this, log diagnostic guidance, and exit cleanly with code 0.*

---

## Building and Running

### Building with CMake
From the repository root:

```powershell
cmake -B build -S . -A x64 -DBUILD_TESTS=ON
cmake --build build --config Release --target stereix_sample_d3d11 stereix_sample_d3d12
```

### Running the Direct3D 11 Sample
```powershell
# Run 60 frames (default):
.\build\bin\stereix_sample_d3d11.exe

# Custom frame count:
.\build\bin\stereix_sample_d3d11.exe --frames 120
```

### Running the Direct3D 12 Sample
```powershell
# Run 60 frames (default):
.\build\bin\stereix_sample_d3d12.exe

# Custom frame count:
.\build\bin\stereix_sample_d3d12.exe --frames 120
```

---

## Commercial Compliance & Anti-Cheat

Binaries linking against `stereix_sdk.dll` operate purely in-process via official DirectX 11/12 APIs:
* **Zero Process Injection**: No memory hooking, proxy DLLs, or `CreateRemoteThread`.
* **Anti-Cheat Safe**: Fully compatible with Easy Anti-Cheat (EAC), BattlEye, and Ricochet.
* **Zero-Crash Hardening**: All internal SDK entry points are wrapped with Structured Exception Handling (SEH) so invalid pointers or runtime disconnects return error codes rather than crashing the host process.
