# Stereix Engine — Unreal Engine 5 & 4 Plugin

**Universal Direct3D 11 & Direct3D 12 VR Adapter with Hardware Depth Submission and Quad-View Foveated Rendering**

---

## 1. Overview

The **Stereix Unreal Engine Plugin** is a high-performance native adapter that connects Unreal Engine (UE 4.27 through UE 5.5+) directly to OpenXR VR runtimes through the Stereix B2B Engine.

### Key Capabilities
- **Direct3D 11 & Direct3D 12 Native Support**: Automatically binds to Unreal's active RHI without modifying engine source code or requiring proprietary private RHI headers.
- **Hardware Depth Submission**: Submits scene depth buffers directly to OpenXR via `XR_KHR_composition_layer_depth`, utilizing Unreal's native Reverse-Z projection for sub-millimeter positional timewarp and occlusion.
- **Multi-Eye & Quad-View Foveated Rendering**: Supports up to 4 views (`STEREIX_MAX_VIEWS 4`), empowering human-eye resolution foveated rendering setups (e.g. Varjo, Pimax Crystal) and spectator broadcast rigs.
- **Lock-Free Render Thread Bridge**: Game Thread stages frames into an atomic ring buffer and dispatches via Unreal's render commands, preventing CPU hitching and compositor deadlocks.
- **Blueprint Function Library**: Inspect session health, runtime depth support, and frame diagnostic stats directly from Blueprints or C++.

---

## 2. Architecture

```text
+-------------------------------------------------------------+
|                     UNREAL ENGINE 5                         |
|                                                             |
|  [Game Thread]                                              |
|    - SceneViewExtension::SetupView                          |
|    - WaitFrame & Head Pose Tracking                         |
|    - Ring Buffer Staging (Stereix_Unreal_StageFrame)        |
|                                                             |
|  [Render Thread]                                            |
|    - SceneViewExtension::PostRenderViewFamily_RenderThread  |
|    - Extract Native Resource (ID3D11Texture2D / D3D12Res)   |
|    - Stereix_Unreal_ProcessRenderCommand                    |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                     STEREIX B2B ENGINE                      |
|  stereix_unreal.dll <--->  stereix_sdk.dll  <--->  OpenXR   |
+-------------------------------------------------------------+
```

---

## 3. Installation & Setup

### Option A: Project Plugin (Recommended)
1. Copy the `Stereix` plugin folder into your project's `Plugins/` directory:
   ```text
   YourProject/
   ├── Plugins/
   │   └── Stereix/
   │       ├── Stereix.uplugin
   │       ├── Binaries/Win64/
   │       ├── Config/
   │       ├── Resources/
   │       └── Source/
   ```
2. In your `.uproject` file, add the plugin entry:
   ```json
   {
       "FileVersion": 3,
       "Plugins": [
           {
               "Name": "Stereix",
               "Enabled": true
           }
       ]
   }
   ```
3. Re-generate project files and compile in Visual Studio / Rider.

### Option B: Engine-Wide Plugin
Copy the `Stereix` plugin folder into:
```text
Engine/Plugins/Marketplace/Stereix/
```

---

## 4. Blueprint & C++ Usage

### Blueprint
Use the `UStereixFunctionLibrary` functions anywhere in your Blueprints:
- **`IsSessionRunning`**: Returns `true` if an active OpenXR session is initialized.
- **`SupportsDepthSubmission`**: Returns `true` if the connected VR headset/runtime supports hardware depth layers.
- **`GetSessionStats`**: Returns live counters for `Submitted`, `Dropped`, `Failed`, and `LastResult`.

### C++ Module Access
```cpp
#include "StereixModule.h"
#include "StereixFunctionLibrary.h"

if (IStereixModule::IsAvailable() && IStereixModule::Get().IsSessionRunning())
{
    // Frame stats
    int32 Submitted, Dropped, Failed, LastResult;
    UStereixFunctionLibrary::GetSessionStats(Submitted, Dropped, Failed, LastResult);

    // 6DOF Controller Tracking & Input
    UStereixFunctionLibrary::SyncInput();
    FStereixControllerState RightHand;
    if (UStereixFunctionLibrary::GetControllerState(1 /* Right */, RightHand))
    {
        if (RightHand.bGripPoseValid)
        {
            // Position weapon or hand mesh
            WeaponMesh->SetWorldTransform(RightHand.GripTransform);
        }

        // Analog trigger & haptic feedback on fire
        if (RightHand.Trigger > 0.85f)
        {
            UStereixFunctionLibrary::TriggerHaptic(1 /* Right */, 50.0f, 160.0f, 0.8f);
        }
    }
}
```

---

## 5. Reverse-Z & Depth Range Specifications

Unreal Engine uses a **Reversed Floating-Point Depth Buffer** (Z_near = 1.0, Z_far = 0.0):
- The plugin automatically signals `STEREIX_DEPTH_RANGE_REVERSED` (`range = 1`) to the OpenXR layer.
- Ensure your project's near clipping plane is set appropriately (default `GNearClippingPlane = 10.0` cm, which corresponds to `0.1` meters).

---

## 6. Distribution Contents

| File | Purpose |
| :--- | :--- |
| `Stereix.uplugin` | Plugin descriptor for Unreal Engine Marketplace & UBT |
| `Binaries/Win64/stereix_sdk.dll` | Engine-agnostic OpenXR runtime core |
| `Binaries/Win64/stereix_unreal.dll`| Native DirectX 11/12 render thread bridge |
| `Source/Stereix/` | Unreal Engine module source code (UBT C++) |
| `Source/ThirdParty/Stereix/` | SDK and bridge header files |
| `Config/FilterPlugin.ini` | Packaging whitelist filter |
| `Resources/Icon128.png` | 128x128 Marketplace display icon |

---

## 7. Support & Licensing

- **Documentation**: [https://stereix-engine.pages.dev/docs](https://stereix-engine.pages.dev/docs)
- **License**: Stereix Commercial B2B Partner License (Mesmeran Lab)
