# Subsystem: Rendering & Stereo VR Pipeline (`src/rendering/`)

The **Rendering** subsystem is responsible for transforming flat 2D game frames into rich stereoscopic 3D images and submitting them to OpenXR headsets within an 11.1 ms frame budget.

---

## Subsystem Architecture

```text
Game Monoscopic Backbuffer + Depth Buffer
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│  Stereo Reprojection Compute Shader                     │
│  (shaders/stereo_reprojection.hlsl)                     │
│  ├── Disocclusion Fill (depth-guided hole patching)     │
│  ├── Color Space Normalization (gamma 2.2 curve)        │
│  └── Interpupillary Separation (IPD Shift)              │
└──────────────────────────┬──────────────────────────────┘
                           │
             ┌─────────────┴─────────────┐
             ▼                           ▼
      [ Left Eye Target ]        [ Right Eye Target ]
             │                           │
             └─────────────┬─────────────┘
                           ▼
┌─────────────────────────────────────────────────────────┐
│  OpenXR Swapchain Submitter                             │
│  (src/openxr/openxr_frame_submitter.cpp)                │
│  └── xrEndFrame -> Headset Display                      │
└─────────────────────────────────────────────────────────┘
```

---

## Key Modules

| Module | Location | Description |
| :--- | :--- | :--- |
| **`DX11GraphicsBackend`** | `dx11/` | DX11 stereo resources, pipeline states, and shadow backbuffers. |
| **`DX12GraphicsBackend`** | `dx12/` | DX12 descriptor heaps, root signatures, compute PSOs, and barrier management. |
| **`VulkanGraphicsBackend`** | `vulkan/` | Vulkan image view lifetimes, command buffers, pipeline caches, and queue sync. |
| **`StereoRenderer`** | `stereo/` | High-level controller dispatching stereo reprojection passes across backends. |
| **`ImGuiVRIntegration`** | `dx11/`, `dx12/`, `vulkan/` | In-headset floating debug dashboard overlay rendered directly onto eye swapchains. |

---

## Critical Stability Rules

- **Fallback Path**: Every backend must implement a monoscopic fallback path. If camera heuristics or depth discovery fail, the raw backbuffer is copied to both eyes so the player sees 2D in-headset rather than a black screen (`BUG-11`).
- **Linearized UNORM Output**: Non-sRGB UNORM swapchains must receive linearized values (`pow(c, 2.2f)`) so the OpenXR compositor's transfer curve produces rich contrast rather than milky, washed-out images (`BUG-14`).
- **Zero-Initialized SubImage Rects**: `XrCompositionLayerProjectionView` subImage structs must explicitly zero `.imageArrayIndex` and `.imageRect.offset` to prevent `XR_ERROR_SWAPCHAIN_RECT_INVALID` (`BUG-15`).
