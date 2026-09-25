# NexVR Engine — Unreal Engine 5 & 4 Plugin

**Universal Direct3D 11 & Direct3D 12 VR Adapter with Hardware Depth Submission and Quad-View Foveated Rendering**

---

## 1. Overview

The **NexVR Unreal Engine Plugin** is a high-performance native adapter that connects Unreal Engine (UE 4.27 through UE 5.5+) directly to OpenXR VR runtimes through the NexVR B2B Engine.

### Key Capabilities
- **Direct3D 11 & Direct3D 12 Native Support**: Automatically binds to Unreal's active RHI without modifying engine source code or requiring proprietary private RHI headers.
- **Hardware Depth Submission**: Submits scene depth buffers directly to OpenXR via `XR_KHR_composition_layer_depth`, utilizing Unreal's native Reverse-Z projection for sub-millimeter positional timewarp and occlusion.
- **Multi-Eye & Quad-View Foveated Rendering**: Supports up to 4 views (`NEXVR_MAX_VIEWS 4`), empowering human-eye resolution foveated rendering setups (e.g. Varjo, Pimax Crystal) and spectator broadcast rigs.
- **Lock-Free Render Thread Bridge**: Game Thread stages frames into an atomic ring buffer and dispatches via Unreal's render commands, preventing CPU hitching and compositor deadlocks.
- **Blueprint Function Library**: Inspect session health, runtime depth support, and frame diagnostic stats directly from Blueprints or C++.

---

## 2. Architecture

```
+-------------------------------------------------------------+
|                     UNREAL ENGINE 5                         |
|                                                             |
|  [Game Thread]                                              |
|    - SceneViewExtension::SetupView                          |
|    - WaitFrame & Head Pose Tracking                         |
|    - Ring Buffer Staging (NexVR_Unreal_StageFrame)          |
|                                                             |
|  [Render Thread]                                            |
|    - SceneViewExtension::PostRenderViewFamily_RenderThread  |
|    - Extract Native Resource (ID3D11Texture2D / D3D12Res)   |
|    - NexVR_Unreal_ProcessRenderCommand                      |
+------------------------------+------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                      NEXVR B2B ENGINE                       |
|  nexvr_unreal.dll  <--->  nexvr_sdk.dll  <--->  OpenXR      |
+-------------------------------------------------------------+
```

---

## 3. Installation & Setup

### Option A: Project Plugin (Recommended)
1. Copy the `NexVR` plugin folder into your project's `Plugins/` directory:
   ```
   YourProject/
   ├── Plugins/
   │   └── NexVR/
   │       ├── NexVR.uplugin
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
               "Name": "NexVR",
               "Enabled": true
           }
       ]
   }
   ```
3. Re-generate project files and compile in Visual Studio / Rider.

### Option B: Engine-Wide Plugin
Copy the `NexVR` plugin folder into:
```
Engine/Plugins/Marketplace/NexVR/
```

---

## 4. Blueprint & C++ Usage

### Blueprint
Use the `UNexVRFunctionLibrary` functions anywhere in your Blueprints:
- **`IsSessionRunning`**: Returns `true` if an active OpenXR session is initialized.
- **`SupportsDepthSubmission`**: Returns `true` if the connected VR headset/runtime supports hardware depth layers.
- **`GetSessionStats`**: Returns live counters for `Submitted`, `Dropped`, `Failed`, and `LastResult`.

### C++ Module Access
```cpp
#include "NexVRModule.h"
#include "NexVRFunctionLibrary.h"

if (INexVRModule::IsAvailable() && INexVRModule::Get().IsSessionRunning())
{
    // Frame stats
    int32 Submitted, Dropped, Failed, LastResult;
    UNexVRFunctionLibrary::GetSessionStats(Submitted, Dropped, Failed, LastResult);

    // 6DOF Controller Tracking & Input
    UNexVRFunctionLibrary::SyncInput();
    FNexVRControllerState RightHand;
    if (UNexVRFunctionLibrary::GetControllerState(1 /* Right */, RightHand))
    {
        if (RightHand.bGripPoseValid)
        {
            // Position weapon or hand mesh
            WeaponMesh->SetWorldTransform(RightHand.GripTransform);
        }

        // Analog trigger & haptic feedback on fire
        if (RightHand.Trigger > 0.85f)
        {
            UNexVRFunctionLibrary::TriggerHaptic(1 /* Right */, 50.0f, 160.0f, 0.8f);
        }
    }
}
```

---

## 5. Reverse-Z & Depth Range Specifications

Unreal Engine uses a **Reversed Floating-Point Depth Buffer** ($Z_{near} = 1.0$, $Z_{far} = 0.0$):
- The plugin automatically signals `NEXVR_DEPTH_RANGE_REVERSED` (`range = 1`) to the OpenXR layer.
- Ensure your project's near clipping plane is set appropriately (default `GNearClippingPlane = 10.0` cm $\rightarrow$ `0.1` m).

---

## 6. Distribution Contents

| File | Purpose |
| :--- | :--- |
| `NexVR.uplugin` | Plugin descriptor for Unreal Engine Marketplace & UBT |
| `Binaries/Win64/nexvr_sdk.dll` | Engine-agnostic OpenXR runtime core |
| `Binaries/Win64/nexvr_unreal.dll`| Native DirectX 11/12 render thread bridge |
| `Source/NexVR/` | Unreal Engine module source code (UBT C++) |
| `Source/ThirdParty/NexVR/` | SDK and bridge header files |
| `Config/FilterPlugin.ini` | Packaging whitelist filter |
| `Resources/Icon128.png` | 128x128 Marketplace display icon |

---

## 7. Support & Licensing

- **Documentation**: [https://nexvr.dev/docs](https://nexvr.dev/docs)
- **License**: NexVR Commercial B2B Partner License
