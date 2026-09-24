# NexVR Engine — Beta Tester Guide (v0.1.90)

Welcome to the **NexVR Engine Closed Beta**. This guide will help you install the engine, launch games in full stereoscopic 3D VR, optimize graphics settings, and report feedback.

---

## 1. Prerequisites & Supported Headsets

### Supported Headsets & Runtimes
- **Meta Quest 2 / 3 / Pro**: Via Link Cable, AirLink, or Virtual Desktop (Vdxr / Oculus OpenXR runtime).
- **Valve Index / HTC Vive / Bigscreen Beyond**: SteamVR OpenXR runtime.
- **Pimax (Crystal, 8KX)**: PimaxXR / SteamVR runtime.
- **Windows Mixed Reality**: Windows Mixed Reality OpenXR runtime.

### System Requirements
- **OS**: Windows 10/11 x64.
- **GPU**: NVIDIA GeForce RTX 2060 / AMD Radeon RX 5700 or higher recommended.
- **DirectX**: DirectX 11 or DirectX 12 compatible title.

---

## 2. Antivirus & Defender Whitelisting

Because NexVR dynamically intercepts graphics calls to create the 3D VR view, Windows SmartScreen or Windows Defender may occasionally flag the injection binaries (`vrinject.dll`, `vr-inject-cli.exe`).

### One-Click Defender Exclusion (PowerShell)
You can run the following command in PowerShell as Administrator to whitelist the installed launcher folder:
```powershell
Add-MpPreference -ExclusionPath "$env:LOCALAPPDATA\Programs\launcher"
```
*(You can also click the **Copy Whitelist Command** button inside the Launcher's **About** panel).*

---

## 3. Recommended In-Game Graphics Settings

For the smoothest and most immersive VR experience, configure these settings inside each game before or after launching:

1. **Display Mode**: **Borderless Windowed** (recommended for seamless injection).
2. **Resolution**: Native 16:9 aspect ratio (e.g. 1920×1080, 2560×1440).
3. **In-Game V-Sync**: **OFF** (NexVR automatically decouples desktop monitor refresh rates, but turning in-game VSync off ensures maximum frame pacing stability).
4. **Motion Blur**: **OFF** (motion blur causes visual smearing in VR headsets).
5. **Depth of Field (DoF)**: **OFF** (DoF blurs distant 3D geometry).

---

## 4. Verified Launch Titles

| Game | Graphics API | Camera / Projection Mode | Notes |
| :--- | :--- | :--- | :--- |
| **Mortal Shell** | DirectX 11 | Full 3D Stereo Reconstruct | Reverse-Z enabled, wide aspect validation |
| **Sekiro: Shadows Die Twice** | DirectX 11 | Full 3D Stereo Reconstruct | 1:1 Display Gamma, standard Z |
| **Hogwarts Legacy** | DirectX 12 | Full 3D Stereo Reconstruct | Reverse-Z enabled, UE4/5 path |
| **Cyberpunk 2077** | DirectX 12 | Full 3D Stereo Reconstruct | DirectML AI depth assisted |
| **Elden Ring** | DirectX 12 | Full 3D Stereo Reconstruct | **Must launch in Offline Mode with EAC disabled** |

---

## 5. In-Headset VR Controls & Dashboard

While inside the game with your headset on:
- **Toggle In-Headset Dashboard**: Press the **Left Controller Menu Button**, gamepad combo **L3 + R3**, or keyboard hotkey **HOME** / **F11** / **INSERT**.
- **Adjust 3D Depth / IPD**: Use the in-headset dashboard to fine-tune 3D separation, eye convergence, and world scale to match your comfort.
- **Recenter View**: Press gamepad **R3 (Hold)** or controller center action.

---

## 6. Anti-Cheat Safety Policy

> [!WARNING]
> **Never inject into competitive online multiplayer games.**
> Titles using kernel-level anti-cheat (Easy Anti-Cheat, BattlEye, Vanguard, Ricochet) are strictly blacklisted. Attempting to force injection into competitive online games can trigger anti-cheat bans on your game accounts. NexVR is designed exclusively for single-player games and offline modding.

---

## 7. Reporting Glitches & Submitting Diagnostics

If you encounter any graphical anomalies, head tracking issues, or crashes:
1. Open the NexVR Launcher.
2. Navigate to **About** or **Settings**.
3. Click **Copy Diagnostics** to copy your system details, hook state, and logs to the clipboard.
4. Share the diagnostics and a description of the issue in the **#beta-feedback** channel on Discord:
   [https://discord.gg/FBeGjgK2fd](https://discord.gg/FBeGjgK2fd)
