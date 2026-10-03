# Stereix Engine — Unity Integration Guide

**High-Performance Native OpenXR Adapter for Unity 2021 LTS, 2022 LTS, and Unity 6**  
Supports **Built-in Render Pipeline**, **Universal Render Pipeline (URP)**, and **High Definition Render Pipeline (HDRP)** on Direct3D 11 & Direct3D 12.

---

## 1. Overview

The **Stereix Unity Adapter** bridges Unity projects directly to OpenXR VR runtimes through `stereix_sdk.dll` and `stereix_unity.dll`. It provides low-overhead, deterministic stereo frame submission, 6DOF controller tracking, analog triggers, and haptic feedback with zero C# garbage collection allocations during rendering.

### Key Features
- **Zero GC Allocations**: Render thread dispatches use native `GL.IssuePluginEventAndData`, avoiding managed-to-unmanaged marshalling overhead during the frame loop.
- **Pipeline Agnostic**: Works out of the box with Built-in Render Pipeline, URP, and HDRP via custom render features or `Camera.onPostRender`.
- **Direct3D 11 & Direct3D 12 Support**: Automatically detects active graphics backend via `SystemInfo.graphicsDeviceType`.
- **Full 6DOF Controller Support**: Pose prediction, tracking status, grip/aim poses, buttons, thumbsticks, and trigger values exposed via `StereixController.cs`.
- **Hardware Depth Submission**: Optional composition layer depth buffer submission for millimetre-accurate timewarp and reprojection.

---

## 2. Directory Structure & Assets

Place the adapter files in your Unity project's `Assets/Stereix/` directory:

```text
Assets/
└── Stereix/
    ├── Plugins/
    │   └── x86_64/
    │       ├── stereix_sdk.dll          (OpenXR runtime engine core)
    │       ├── stereix_unity.dll        (Native Unity GfxDevice bridge)
    │       ├── openxr_loader.dll        (Khronos OpenXR loader)
    │       └── vrinject.dll             (Optional interceptor payload)
    └── Scripts/
        ├── Stereix.cs                   (C# P/Invoke ABI bindings & constants)
        ├── StereixCamera.cs             (Stereo camera rig & frame submitter)
        └── StereixController.cs         (6DOF motion controller handler)
```

> **Plugin Inspector Settings in Unity:**
> Select all DLLs under `Plugins/x86_64/` and ensure **Platform: Standalone**, **CPU: x86_64**, and **OS: Windows** are checked.

---

## 3. Quick Start (Rigging a Camera in 3 Steps)

### Step 1: Add Stereix Manager & Camera
1. Create an empty GameObject in your scene named `StereixRig`.
2. Add a child Camera GameObject named `StereixCamera`.
3. Attach `StereixCamera.cs` to the camera GameObject.
4. Set the camera target texture to a `RenderTexture` matching your headset's recommended resolution (e.g. `3496x1944`, `DXGI_FORMAT_R8G8B8A8_UNORM`).

### Step 2: Configure Controller Trackers
1. Create two child GameObjects under `StereixRig`: `LeftHand` and `RightHand`.
2. Attach `StereixController.cs` to each:
   - For `LeftHand`, set `Hand = Stereix_Hand.Left`.
   - For `RightHand`, set `Hand = Stereix_Hand.Right`.
3. Assign your hand or tool meshes as children of these GameObjects.

### Step 3: Run the Scene
Press **Play**. The Stereix adapter will:
1. Query VR graphics requirements and headset resolution.
2. Initialize the OpenXR session on the active D3D11/D3D12 device.
3. Drive camera poses and controller tracking at the native compositor cadence (e.g. 90 Hz / 120 Hz).

---

## 4. Scripting API Examples

### Initializing and Querying Session State
```csharp
using UnityEngine;
using Stereix;

public class VRSessionManager : MonoBehaviour
{
    void Start()
    {
        if (StereixEngine.IsAvailable())
        {
            var reqs = StereixEngine.GetGraphicsRequirements();
            Debug.Log($"Stereix VR Target Resolution: {reqs.requiredTextureWidth}x{reqs.requiredTextureHeight}");
        }
        else
        {
            Debug.LogWarning("Stereix SDK runtime not detected; defaulting to desktop 2D.");
        }
    }
}
```

### Reading 6DOF Controller Tracking & Triggering Haptics
```csharp
using UnityEngine;
using Stereix;

public class WeaponFire : MonoBehaviour
{
    [SerializeField] private Stereix_Hand hand = Stereix_Hand.Right;

    void Update()
    {
        Stereix_ControllerState state;
        if (StereixEngine.GetControllerState(hand, out state) && state.isConnected)
        {
            // Check analog index trigger pull
            if (state.trigger > 0.85f)
            {
                ShootWeapon();
                // Trigger 50ms vibration at 160Hz frequency and 80% amplitude
                StereixEngine.TriggerHaptic(hand, durationSeconds: 0.05f, frequencyHz: 160.0f, amplitude: 0.8f);
            }

            // Check thumbstick click or button press
            if ((state.buttonsDown & Stereix_ButtonMask.PrimaryButton) != 0)
            {
                ReloadWeapon();
            }
        }
    }

    void ShootWeapon() { /* Fire projectile */ }
    void ReloadWeapon() { /* Reload ammo */ }
}
```

---

## 5. Universal Render Pipeline (URP) Integration

For URP projects, submit the render target using a `ScriptableRendererFeature` or directly in `RenderPipelineManager.endCameraRendering`:

```csharp
void OnEnable()
{
    RenderPipelineManager.endCameraRendering += OnEndCameraRendering;
}

void OnDisable()
{
    RenderPipelineManager.endCameraRendering -= OnEndCameraRendering;
}

void OnEndCameraRendering(ScriptableRenderContext context, Camera cam)
{
    if (cam == vrCamera && vrSession != System.IntPtr.Zero)
    {
        StereixEngine.SubmitFrameDX11(vrSession, currentFrameToken, renderTarget.GetNativeTexturePtr());
    }
}
```

---

## 6. Anti-Cheat & Production Compatibility

- **Direct In-Engine API**: Links via standard P/Invoke into `stereix_sdk.dll`. Zero DLL injection or code detouring is used.
- **EAC / BattlEye Safe**: Compliant with anti-cheat engines across Steam and Epic Games Store distributions.
- **Zero-Crash SEH Guarding**: All native C functions are protected with Structured Exception Handling. Disconnecting a headset or runtime during gameplay yields clean error codes (`STEREIX_SESSION_EXITING`) instead of crashes.

---

## 7. Commercial Support

- **Repository**: [https://github.com/sathishssj3/Stereix-Engine](https://github.com/sathishssj3/Stereix-Engine)
- **Discord Community**: [https://discord.gg/FBeGjgK2fd](https://discord.gg/FBeGjgK2fd)
- **Publisher**: Mesmeran Lab (Copyright © 2026)
