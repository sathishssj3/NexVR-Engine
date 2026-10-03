# StereixSampleUE5 — Turnkey Unreal Engine 5 PCVR Sample Project

This sample project demonstrates how to enable instant, zero-injection PCVR rendering in any Unreal Engine 5 (UE 5.1 through 5.5+) game project using the **Stereix B2B Engine Plugin**.

---

## What's Included

* **`StereixSampleUE5.uproject`**: Ready-to-open UE5 project configured with the `Stereix` plugin enabled.
* **`Plugins/Stereix/`**: Full native C++ plugin utilizing Unreal's `ISceneViewExtension` to intercept rendered frames and submit them to OpenXR with sub-millimeter positional timewarp.
* **`Config/DefaultEngine.ini`**: Tuned VR rendering settings (Forward/Deferred shading, optimal Anti-Aliasing, Reverse-Z depth capture).

---

## 2-Minute Quickstart

### Step 1: Open in Unreal Engine 5
1. Right-click `StereixSampleUE5.uproject` -> **Generate Visual Studio project files**.
2. Open `StereixSampleUE5.sln` in Visual Studio 2022 or JetBrains Rider.
3. Build the project in **Development Editor** configuration (Win64).
4. Launch the project in Unreal Editor.

### Step 2: Play in VR
1. Ensure your OpenXR runtime is running (Meta Quest Link, SteamVR, Virtual Desktop, or Pico).
2. In the Unreal Editor toolbar, click the dropdown next to **Play** and select **VR Preview** (or press `Alt + P`).
3. Put on your headset. You will see 6DOF head-tracked stereo rendering at the HMD's native refresh rate (90 / 120 Hz).

---

## How It Works in Your Own Game

To add Stereix to your existing Unreal Engine project:
1. Copy `Plugins/Stereix` to your project's `Plugins/` folder.
2. In `YourGame.uproject`, ensure `"Plugins": [ { "Name": "Stereix", "Enabled": true } ]` is set.
3. Done. The plugin's `FStereixViewExtension` automatically registers with Unreal's render pipeline on startup. No game code modifications required.
