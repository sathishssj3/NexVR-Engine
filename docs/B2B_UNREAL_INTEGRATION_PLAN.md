# B2B Phase 2.1 — Unreal Engine Integration Plan

**Status:** Native bridge landed and tested. Unreal Plugin scaffolding (`NexVR.uplugin`, `FNexVRViewExtension`) drafted.
**Artifacts:** `nexvr_unreal_bridge.cpp` → `nexvr_unreal.dll`, `NexVR.uplugin`, `Source/NexVR/`

---

## 1. The problem in one paragraph

`NexVR_SubmitFrameWithDepthDX11` must run on the thread that owns the Direct3D 11 immediate context. In Unreal Engine that is the **Render Thread**, while gameplay actor ticking, pawn input, and camera updates run on the **Game Thread**. Unreal's sanctioned crossings are `ENQUEUE_RENDER_COMMAND` and `FSceneViewExtensionBase::PostRenderViewFamily_RenderThread`. The integration is architected specifically around those crossings to guarantee zero GPU stalls and thread safety.

---

## 2. Thread mapping

| Unreal thread | SDK / Bridge call | Why it belongs there |
|---|---|---|
| **Game Thread** (`StartupModule`) | `NexVR_GetGraphicsRequirements` | Must precede viewport / device binding |
| **Game Thread** | `NexVR_InitializeDX11` | Needs the device recovered from the viewport target; runs once |
| **Game Thread** (`SetupView`) | `NexVR_WaitFrame` | Blocks for display interval; returns head tracking poses and token |
| **Game Thread** (`PostRender`) | `NexVR_Unreal_StageFrameWithDepth` | Hands the token, color texture, and depth buffer to the staging ring |
| **Render Thread** (`ENQUEUE_RENDER_COMMAND`) | `NexVR_Unreal_ProcessRenderCommand` | Copies textures under D3D11 immediate context lock and calls `xrEndFrame` |
| **Game Thread** (`ShutdownModule`) | `NexVR_Unreal_Detach` → `NexVR_Shutdown` | Detach first, always — see §6 |

### Per-frame sequence

```
GAME THREAD (SetupView)              RENDER THREAD (ENQUEUE_RENDER_COMMAND)
───────────────────────              ──────────────────────────────────────
NexVR_WaitFrame(&state)
  └─ blocks ~11.8 ms (the throttle)
  └─ returns token + head poses
apply poses to FSceneView
render scene to Viewport / RenderTarget

slot = StageFrameWithDepth(token, ColorRHI, DepthRHI, NearZ, FarZ, Reversed)
  └─ -1 means skip this frame,
     never spin waiting
ENQUEUE_RENDER_COMMAND(NexVRSubmit)(slot) ──► OnRenderCommand(slot)
                                                ├─ read token, color, depth
                                                └─ NexVR_SubmitFrameWithDepthDX11
                                                     ├─ acquire  (no D3D lock)
                                                     ├─ wait     (no D3D lock, finite)
                                                     ├─ LOCK → CopyResource (color + depth) → UNLOCK
                                                     └─ release, xrEndFrame
```

---

## 3. Why the bridge vendors no Unreal headers

The bridge consumes standard Direct3D 11 pointers (`ID3D11Texture2D*`).

- An `FRHITexture2D` from Unreal's RHI exposes `(ID3D11Texture2D*)TextureRHI->GetNativeResource()`.
- The native texture already knows the `ID3D11Device` it was created on via `ID3D11Texture2D::GetDevice()`.
- `NexVR_Unreal_GetDeviceFromTexture` wraps this, so the bridge needs no Unreal internal RHI headers and has zero compile-time coupling to specific Unreal Engine minor versions.

---

## 4. Reverse-Z depth buffer submission

Unreal Engine uses **Reverse-Z** floating point depth (`GReversedZProject = true`) standardly on Direct3D 11, Direct3D 12, and Vulkan.

1. **Near/Far convention:** Near plane is mapped to 1.0, and far plane is mapped to 0.0 in the Z-buffer.
2. **Depth range flag:** `NexVR_Unreal_StageFrameWithDepth` passes `depthRange = 1` (`NEXVR_DEPTH_RANGE_REVERSED`), allowing OpenXR runtimes implementing `XR_KHR_composition_layer_depth` to linearize and reproject with high precision.
3. **Clip distances:** `nearZ` and `farZ` correspond to `GNearClippingPlane / 100.0f` (converting Unreal centimeters to meters) and the camera's far distance.

---

## 5. Plugin Architecture (`src/adapters/unreal/`)

The adapter is structured as a standard Unreal Engine plugin:

```
src/adapters/unreal/
├── CMakeLists.txt              # Native DLL build (nexvr_unreal.dll)
├── nexvr_unreal_bridge.h       # C ABI header
├── nexvr_unreal_bridge.cpp     # Lock-free staging ring implementation
├── NexVR.uplugin               # Unreal plugin descriptor
└── Source/
    └── NexVR/
        ├── NexVR.Build.cs      # UBT module definition
        ├── Public/
        │   ├── NexVRModule.h
        │   ├── NexVRViewExtension.h
        │   └── NexVRFunctionLibrary.h
        └── Private/
            ├── NexVRModule.cpp
            ├── NexVRViewExtension.cpp
            └── NexVRFunctionLibrary.cpp
```

### Key Classes

- **`INexVRModule` (`IModuleInterface`)**: Engine module lifecycle, handles initialization and binds OpenXR session.
- **`FNexVRViewExtension` (`FSceneViewExtensionBase`)**: Injects head tracking into `FSceneView` and intercepts `PostRenderViewFamily_RenderThread` for swapchain submission.
- **`UNexVRFunctionLibrary` (`UBlueprintFunctionLibrary`)**: Blueprint nodes for querying tracking status, session statistics, and toggling depth submission.

---

## 6. Shutdown ordering

`NexVR_Unreal_Detach` **before** `NexVR_Shutdown`, always.

Unreal's Render Thread runs asynchronously behind the Game Thread. Calling `NexVR_Shutdown` while an `ENQUEUE_RENDER_COMMAND` is still queued would cause the Render Thread to execute against freed OpenXR session memory. Calling `NexVR_Unreal_Detach` resets the bridge's session pointer under a release store and frees all ring slots, ensuring late render commands safely no-op.

---

## 7. Multi-GPU & Adapter selection

If a machine has multiple GPUs (e.g. integrated Intel/AMD GPU + discrete NVIDIA GPU):
- Run Unreal Engine with `-graphicsadapter=N` (or `-preferredgpu=N`) to force the engine onto the same GPU driving the VR headset.
- If a mismatch occurs, `NexVR_InitializeDX11` returns `NEXVR_ERROR_WRONG_ADAPTER` with the required adapter LUID.
