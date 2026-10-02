# NexVR Client Architecture & Engineering Specifications

## 1. Native Windows Engine (`nexvr-client`)

The native engine is engineered for low-latency graphics injection, frame interception, and OpenXR stereo frame generation.

```
                  ┌──────────────────────────────┐
                  │    Target Game (e.g. Sekiro) │
                  └──────────────┬───────────────┘
                                 │
                 DirectX/Vulkan API Calls Intercepted
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │     HookManager (MinHook)    │
                  ├──────────────────────────────┤
                  │  • Present / Present1 Hook   │
                  │  • ResizeBuffers Hook        │
                  │  • DrawIndexed Hook          │
                  │  • XInput / Mouse Hook       │
                  └──────────────┬───────────────┘
                                 │
                  ┌──────────────┴───────────────┐
                  ▼                              ▼
      ┌───────────────────────┐      ┌────────────────────────┐
      │ Stereo Reprojection   │      │ AI Neural Inpainting   │
      ├───────────────────────┤      ├────────────────────────┤
      │ • Depth Buffer Hook   │      │ • DirectML / ONNX      │
      │ • Left/Right Camera   │      │ • Disocclusion Fill    │
      │ • Stereo Warp CS      │      │ • Async Inference Pool │
      └───────────┬───────────┘      └───────────┬────────────┘
                  │                              │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │   OpenXR Frame Submitter     │
                  ├──────────────────────────────┤
                  │  • xrWaitFrame               │
                  │  • xrBeginFrame              │
                  │  • xrEndFrame (2 Views)      │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                      Virtual Reality Headset
```

---

## 2. Core Subsystems

### 2.1 Hook Manager & API Interception
- Utilizes **MinHook** for atomic function prologue redirection.
- Wraps `IDXGISwapChain::Present`, `IDXGISwapChain::ResizeBuffers`, `ID3D12CommandQueue::ExecuteCommandLists`, and `vkQueuePresentKHR`.
- Avoids multiple initialization or teardown calls to MinHook (owned exclusively by `HookManager`).

### 2.2 Stereo Reprojection & Depth Extraction
- Intercepts game depth buffers (`DXGI_FORMAT_D32_FLOAT`, `DXGI_FORMAT_D24_UNORM_S8_UINT`).
- Runs compute shaders (`stereo_warp.hlsl`, `depth_reprojection.hlsl`, `bilateral_blur.hlsl`) to generate synthetic left/right eye views from the single monoscopic game frame.
- Analyzes depth buffer inversion (Reverse-Z vs standard depth) automatically at runtime.

### 2.3 OpenXR Runtime Integration
- Initializes OpenXR session (`XR_REFERENCE_SPACE_TYPE_LOCAL` or `STAGE`).
- Manages dual swapchains (left eye, right eye) matching the native display resolution of connected VR headsets (e.g., Quest 3, Index, Bigscreen Beyond).
- Handles asynchronous frame timing with Asynchronous Spacewarp (ASW) fallback if the target game dips below 90 FPS.

### 2.4 Electron Desktop Launcher (`nexvr-client/launcher`)
- **Framework**: Vite, React, TypeScript, Electron.
- **Features**:
  - Automatically scans Steam, Epic, and custom directories for installed games.
  - Matches executable signatures with the local/cloud game profiles.
  - Deploys `vrinject.dll` and dependencies next to the target executable or injects on launch.
  - Seamless background OTA updates via signed GitHub releases and CloudFront.
