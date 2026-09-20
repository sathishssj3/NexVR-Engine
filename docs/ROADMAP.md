# NexVR Engine — Comprehensive Roadmap & Architecture Guide
**Universal Stereoscopic VR & 6DOF Motion Controller Injection Runtime**

This document tracks all completed technical achievements and details the future engineering roadmap for transforming flat-screen PC games into native, first-person VR experiences with 6DOF motion controller weapon handling.

---

## 1. Executive Summary & Core Philosophy

NexVR Engine is engineered around a unified, single-binary injection model (`vrinject.dll` deployed via `vr-inject-cli.exe` and managed through our modern Electron/React Launcher). 

*   **Universal Graphics & Spatial Audio/Compositor:** Handled at the hardware level across DirectX 11, DirectX 12, and Vulkan via OpenXR 1.0.34.
*   **Game-Specific Logic & Weapon Handling:** Handled via built-in Engine Auto-Adapters (Unreal, Unity), a high-speed Heuristic Memory Scanner, and Declarative JSON Profiles (`profiles/<game_id>.json`).

---

## 2. Completed Milestones (Verified & Shipped)

### ✅ Phase 1: Universal Graphics & Swapchain Hook Engine
*   **Triple-API Interception:**
    *   **DirectX 11:** Low-latency VTable hooking (`IDXGISwapChain::Present`, `ResizeBuffers`, device context capture).
    *   **DirectX 12:** Command queue interception (`ExecuteCommandLists`, `Present1`) with dummy-device vtable safety.
    *   **Vulkan:** Direct layer & detour hooks (`vkQueuePresentKHR`, `vkCreateSwapchainKHR`, `vkAcquireNextImageKHR`).
*   **API Misidentification Defense (BUG-10):**
    *   Direct buffer probing (`GetBuffer(0, IID_PPV_ARGS(&probe))`) to distinguish real DX12 games from DX11 games creating background compute queues (e.g., *Sekiro: Shadows Die Twice*).
*   **MinHook Lifecycle Management (BUG-01/02):**
    *   Centralized `HookManager` orchestration preventing double-initialization and multi-threaded hooking deadlocks.

### ✅ Phase 2: Spatial Compositor & OpenXR 1.0.34 Bridge
*   **Zero-Latency Native Frame Submission:**
    *   Direct pipeline binding to OpenXR runtimes (`XrSession`, `XrSwapchain`, `xrEndFrame`).
    *   Full compatibility across Meta Quest Link / AirLink / Virtual Desktop, SteamVR, HTC Vive, and Windows Mixed Reality.
*   **OpenXR Sub-Image Rect Protection (BUG-15):**
    *   Explicit zero-initialization preventing `XR_ERROR_SWAPCHAIN_RECT_INVALID`.
*   **Monoscopic Zero-Crash Fallback (BUG-11):**
    *   Automatic hardware blit (`CopyResource` / `CopyTextureRegion` / `vkCmdCopyImage`) to headset if stereo heuristics fail, ensuring the player never experiences a black screen.

### ✅ Phase 3: Hardware-Accelerated Shaders & Tonemapping
*   **Compute Shader Pipelines:**
    *   High-performance HLSL shaders compiled at build time (`fxc` cs_5_0 for DX11, `dxc` cs_6_0 for DX12, `glslc` SPIR-V for Vulkan).
    *   Real-time user adjustment of Interpupillary Distance (IPD), eye convergence, and world scale.
*   **Color Space & Tonemapping Invariance (BUG-17):**
    *   Pure pass-through sRGB tonemapping without artificial `pow(c, 2.2)` squaring, preventing black crush and preserving original game lighting.

### ✅ Phase 4: Heuristics & Memory Scanning Infrastructure
*   **Matrix Classifier (`MatrixClassifier`):**
    *   Automated heuristic detection of View and Projection matrices in dynamic GPU constant buffers.
*   **Dynamic Heap Scanner (`PageScanner`, `PointerChainResolver`):**
    *   Traverses `MEM_COMMIT` writable memory regions using structured SEH protection to resolve multi-level static base pointer chains (e.g. `Game.exe + 0x3AF10 -> +0x24 -> +0x180`).
*   **Camera Delta Tracker:**
    *   Correlates memory candidate values with mouse and head deltas to lock onto true camera vectors.

### ✅ Phase 5: Secure Elevated CLI Injector (`vr-inject-cli.exe`)
*   **Reliable DLL Injection:**
    *   Native x64 injection using `CreateRemoteThread` + `LoadLibraryW`.
    *   Mandatory UAC elevation manifest (`requireAdministrator`).
    *   Cryptographic handshake token (`NEXVR_AUTH_TOKEN`) preventing unauthorized DLL loading.
    *   Anti-cheat tripwire checks preventing accidental injection into strict anti-cheat protected titles.

### ✅ Phase 6: Modern Workstation Launcher (Electron + React + Vite)
*   **Fluid Desktop UI/UX:**
    *   Obsidian dark-mode aesthetic with fluid layouts and zero window clipping on all supported aspect ratios.
    *   Automated library scanner for Steam, Epic Games Store, Xbox Game Pass, and custom executables.
    *   Live log streaming, one-click Windows Defender PowerShell exclusion configuration, and system diagnostic export.
    *   Instant `app.asar` live-patcher (`npm run sync:installed`) for rapid development and testing.
    *   Strict proprietary branding and source-code protection.

### ✅ Phase 7: Curated Game Profile Library
*   Verified declarative JSON profiles for 12+ AAA titles:
    *   *Cyberpunk 2077*, *Elden Ring*, *Sekiro: Shadows Die Twice*, *No Man's Sky*, *Palworld*, *Mortal Shell*, *Atomic Heart*, *Hogwarts Legacy*, *Fallout 4*, *Skyrim Special Edition*, *Monster Hunter: World*, and *Valheim*.

---

## 3. Future Roadmap: 6DOF Motion Controls & First-Person Gameplay

```
                        ┌─────────────────────────────────────────┐
                        │        OpenXR 1.0.34 Runtime            │
                        │    HMD Pose  +  Left / Right Hands      │
                        └──────────────────┬──────────────────────┘
                                           │
                    ┌──────────────────────┴──────────────────────┐
                    ▼                                             ▼
        [ HMD Camera Controller ]                   [ 6DOF Hand & Motion Engine ]
        • 1st-Person Camera Placement               • OpenXR Action Spaces (/user/hand/*)
        • Head Mesh Visibility Culling              • Linear & Angular Velocity Tracking
                    │                                             │
                    └──────────────────────┬──────────────────────┘
                                           │
                                           ▼
               ┌───────────────────────────────────────────────────────┐
               │         Weapon Attachment & Interaction Engine        │
               ├───────────────────────────┬───────────────────────────┤
               │   Method A: Skeletal Bone │   Method B: Viewmodel     │
               │   Hijacking & IK Solver   │   Matrix Detachment       │
               │   (Elden Ring, RE5)       │   (DOOM, Bioshock, FPS)   │
               └─────────────┬─────────────┴─────────────┬─────────────┘
                             │                           │
                             ▼                           ▼
                 [ Physical Melee/Shield ]     [ Weapon Fire Raycast ]
                 • Velocity Impact Detection   • Ray Origin: Muzzle Socket
                 • Angle-based Shield Block    • Ray Dir: Controller Vector
```

---

### 🚀 Phase 8: OpenXR 6DOF Controller Tracking & Input Emulation
*   **OpenXR Action Sets:**
    *   Initialize action spaces for `/user/hand/left` and `/user/hand/right`.
    *   Query `xrLocateSpace` each frame for controller Position `(X, Y, Z)`, Orientation Quaternion `(Qx, Qy, Qz, Qw)`, Linear Velocity, and Angular Velocity.
*   **Virtual Input Bridge:**
    *   Map controller thumbsticks, triggers, and grip buttons to virtual XInput (gamepad) or Raw Input (mouse/keyboard) events for seamless zero-config movement.

---

### 🚀 Phase 9: First-Person Camera Decoupling & Head Culling
*   **True 1st-Person Perspective:**
    *   Hook 3rd-person game cameras (*Elden Ring*, *Resident Evil 5*) and anchor the camera position directly to the character's **Head / Eye bone**.
*   **Head Mesh Visibility Culling:**
    *   In the vertex shader or via index-buffer clipping, hide the player character's head/neck geometry so the player never sees the inside of their own character's teeth, hair, or eyeballs.
*   **Pitch/Yaw Decoupling:**
    *   Decouple head orientation from body rotation, allowing natural looking around while walking in any direction.

---

### 🚀 Phase 10: Universal Weapon Attachment & Skeletal Bone Hijacking
*   **Engine Auto-Adapters (Built into `vrinject.dll`):**
    *   **Unreal Engine Adapter (UE4 / UE5):** Auto-hook `USkeletalMeshComponent` and `GetSocketTransform("WeaponSocket")` to auto-bind weapons across hundreds of Unreal Engine games.
    *   **Unity Adapter:** Auto-hook `UnityPlayer.dll` `Transform` and `Animator` bones.
*   **Skeletal Bone Hijacking for Proprietary Engines:**
    *   Hook the global bone matrix calculation pass in games with custom engines (*Elden Ring*, *Resident Evil*, *Dark Souls*).
    *   Overwrite the Right Hand / Weapon Bone matrix with the OpenXR Right Controller transform.
    *   Overwrite the Left Hand / Shield Bone matrix with the OpenXR Left Controller transform.
*   **FPS Viewmodel Matrix Detachment:**
    *   For flat FPS titles (*DOOM*, *BioShock*, *Halo*), identify viewmodel draw calls during rendering.
    *   Replace `ViewMatrix_Camera` with `ViewMatrix_RightHand`, detaching the weapon from the player's face and anchoring it to the physical touch controller.
*   **Two-Bone Analytical IK (Inverse Kinematics):**
    *   A high-performance C++ IK solver that calculates the natural bend of the character's elbow and shoulder, ensuring arms realistically reach out toward the controller.

---

### 🚀 Phase 11: Combat, Gunplay & Physical Melee Physics
*   **Muzzle Raycast Redirection:**
    *   Hook the game's weapon fire / bullet spawn routine (`TraceLine` / `RaycastWorld`).
    *   Replace the origin with the physical weapon muzzle socket and the direction with the controller forward vector. Bullets travel exactly where the player points their wrist.
*   **Physical Melee & Shield Mechanics:**
    *   Monitor controller linear velocity to register sword, hammer, and axe swings (e.g. speed $> 1.8\text{ m/s}$ triggers melee hit event).
    *   Use the Left Controller's orientation plane for directional shield blocking.
*   **Two-Handed Rifle Stabilization:**
    *   Compute the aiming direction between primary and secondary hands:
        $$\text{AimDirection} = \text{normalize}(\text{LeftHandPosition} - \text{RightHandPosition})$$
    *   Gives authentic two-handed stabilization for rifles, shotguns, and sniper scopes.

---

### 🚀 Phase 12: In-Headset VR Configurator HUD (ImGui 3D Overlay)
*   **In-VR Spatial Menu:**
    *   Toggled via controller shortcut (e.g. `Left Stick Click + Right Stick Click`).
    *   Displays detected character bones, camera height offsets, IPD adjustments, and weapon angle sliders.
    *   Allows modders and players to click *"Attach Camera to Head"* and *"Attach Weapon to Right Hand"*, saving the result directly to `profiles/<game_id>.json` without restarting the game.

---

### 🚀 Phase 13: DirectML Stage 2 Real-Time Neural Inpainting
*   **AI Edge Disocclusion Patching:**
    *   Integrate real ONNX DirectML model weights for our U-Net depth-guided inpainter.
    *   Execute under a strict `< 1.5 ms` GPU budget on the render thread to seamlessly patch disocclusion holes without boundary color bleeding.

---

## 4. Architectural Summary

| Layer | Responsibility | Status |
| :--- | :--- | :--- |
| **Graphics & Swapchain Hooks** | DX11, DX12, Vulkan swapchain capture & Present hooking | **Complete (100%)** |
| **OpenXR Compositor** | OpenXR 1.0.34 frame submission, HMD head tracking, IPD | **Complete (100%)** |
| **Desktop Launcher & CLI** | Process scanning, UAC injector CLI, telemetry, profiles | **Complete (100%)** |
| **Curated AAA Profiles** | 12+ verified game configurations | **Complete (100%)** |
| **6DOF Controller Tracking** | OpenXR action sets, hand poses, velocity vectors | **Planned (Phase 8)** |
| **1st-Person Camera Decoupling** | Head bone attachment, head mesh culling, yaw decoupling | **Planned (Phase 9)** |
| **Universal Weapon Attachment** | Engine adapters, bone hijacking, viewmodel detachment, IK | **Planned (Phase 10)** |
| **Combat & Gunplay Physics** | Muzzle raycasting, velocity swing detection, two-handed rifle | **Planned (Phase 11)** |
| **In-VR HUD Configurator** | Floating 3D ImGui menu for live bone and weapon adjustment | **Planned (Phase 12)** |
| **DirectML Neural Inpainter** | Depth-guided AI disocclusion hole patching (< 1.5 ms) | **Planned (Phase 13)** |
