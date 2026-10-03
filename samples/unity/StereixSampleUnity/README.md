# StereixSampleUnity — Turnkey Unity PCVR Sample Project

This sample project demonstrates how to turn any standard flat Unity project into a high-performance OpenXR PCVR title in 60 seconds using the **Stereix B2B Unity Bridge**.

---

## What's Included

* **`Assets/Stereix/Plugins/x86_64/`**: Native Direct3D 11/12 bridging binaries (`stereix_unity.dll`, `stereix_sdk.dll`).
* **`Assets/Stereix/Scripts/`**:
  * **`StereixCamera.cs`**: Attach to any Unity `Camera` to drive stereo eye poses from OpenXR, submit render textures, and send depth buffers.
  * **`StereixController.cs`**: Drives 6DOF motion controller tracking, grip/aim poses, button bitmasks, analog triggers, and haptic pulses.
  * **`Stereix.cs`**: Pure C# P/Invoke declarations and data structures matching the official Stereix C ABI.
* **`Assets/Scenes/StereixSampleScene.unity`**: Pre-configured sample scene ready for instant Play-Mode testing.

---

## 2-Minute Quickstart

### Step 1: Open in Unity
1. Launch **Unity Hub**.
2. Click **Add** -> **Add project from disk**.
3. Select the `StereixSampleUnity` folder.
4. Open with Unity **2021.3 LTS**, **2022.3 LTS**, or **Unity 6 (6000.x)**.

### Step 2: Press Play
1. In the Project window, double-click `Assets/Scenes/StereixSampleScene.unity` to open the sample scene.
2. Ensure your OpenXR VR headset is connected (Meta Quest Link, SteamVR, Virtual Desktop, or Pico).
3. Press **Play** (`Ctrl + P`).
4. Put on your headset. You will see 6DOF stereoscopic rendering with low-latency timewarp.

---

## Adding Stereix to Your Existing Unity Project

1. Copy the `Assets/Stereix/` folder into your existing project's `Assets/` directory.
2. Select your Main Camera in the hierarchy.
3. In the Inspector, click **Add Component** -> search for **Stereix VR Camera**.
4. Press Play. Your game is now running in VR.
