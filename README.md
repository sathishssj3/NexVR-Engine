# NexVR Engine Injector

![NexVR Engine Launcher](launcher/dist/assets/icon.ico)

NexVR Engine is a high-performance Universal VR Injector built to bring native OpenXR support to traditional flat-screen PC games. Featuring a seamless Electron/React GUI and a robust C++ injection engine, NexVR Engine hooks into DX11, DX12, and Vulkan render pipelines to automatically submit frames to your VR headset.

## Key Features

- **Universal Graphics Support**: Hooks directly into DirectX 11, DirectX 12, and Vulkan swapchains.
- **Smart Steam Integration**: Automatically scans your local Steam libraries and detects compatible installations.
- **Auto Headset Detection**: Leverages the Windows Registry to automatically detect your active OpenXR runtime (Meta Quest, SteamVR, WMR).
- **Live Session Logs**: Streams real-time injection and hooking logs directly to the launcher UI.
- **Per-Game Configuration**: Save individual VR configurations, resolutions, and input settings for every game in your library.

## Installation

1. Download the latest `NexVR Engine Setup.exe` from the Releases page.
2. Run the installer. By default, it will install to your local AppData directory.
3. Launch the **NexVR Engine** application from your Start Menu.

## How to Use

1. **Open the Launcher**: Ensure your VR Headset is connected and your OpenXR runtime (e.g., Oculus App or SteamVR) is running. The bottom-left status indicator should glow green.
2. **Select a Game**: Browse your auto-detected Steam library on the left sidebar.
3. **Configure Settings**: Adjust the VR-specific settings for the selected game.
4. **Inject**: Click the **▶ INJECT** button. The launcher will automatically start the game via Steam, locate its process, and inject the VR `.dll`.
5. **Put on your Headset**: Once injected successfully, the game will begin submitting frames directly to your headset!

## Configuration Settings

| Setting | Description |
|---|---|
| **Use Recommended Resolution** | Automatically overrides the game's internal render resolution to match your headset's native panel resolution for optimal clarity. |
| **Motion Sensitivity** | Adjusts how strongly head-tracking and controller movements translate to in-game camera movement. |
| **Raw Input Mode** | Bypasses standard Windows input processing for lower latency motion-controller tracking. |
| **sRGB Correction** | Applies a gamma correction curve to the final VR output to prevent washed-out colors. |
| **Depth Submission** | Submits the depth buffer to the OpenXR runtime, enabling advanced features like SpaceWarp and better reprojection. |
| **Auto-Inject on Launch** | If enabled, the launcher will automatically attempt to inject into the game the moment it detects the executable running. |

## Troubleshooting

- **Game crashes immediately on Inject**: Ensure you do not have conflicting overlays enabled (e.g., Discord Overlay, MSI Afterburner, RivaTuner). These can interfere with the DX/Vulkan hooks.
- **UI shows "Game path not found"**: Try clicking **Rescan Library** at the bottom of the sidebar.
- **Injection succeeds but headset is black**: Check the *Live Session Logs* in the UI. If the hook failed to find the SwapChain, try changing the game to "Windowed Mode" or "Borderless" in its native settings.
- **No Headset Detected**: Verify that your OpenXR runtime is set correctly in the Windows Registry (`HKLM\SOFTWARE\Khronos\OpenXR\1`).

## Development & Building

If you wish to build NexVR Engine from source:

```bash
# 1. Build the C++ DLL and CLI
mkdir build && cd build
cmake ..
cmake --build . --config Release

# 2. Build and Package the Launcher
cd ../launcher
npm install
npm run package
```

---
*Developed by the NexVR Team.*
