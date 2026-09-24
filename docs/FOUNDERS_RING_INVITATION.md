# NexVR Engine — Founder's Ring (Phase 1 VIP Alpha) Invitation

*Copy-pasteable message for Discord (#founders-beta or DMs) or email to the initial 5–10 beta testers.*

---

```markdown
👋 **Welcome to the NexVR Engine Closed Beta (Founder's Ring / Phase 1)!**

Thank you for being one of our first hand-picked testers. You are receiving early access to **NexVR Engine v0.1.90-beta**, a universal injection engine that converts standard flat PC games into immersive, stereoscopic 3D VR experiences with real-time camera tracking.

---

### 🚀 1. Download & Installation

Choose either the installer or the standalone portable package:

* 📦 **Standard Installer**: `NexVR-Engine-Setup-0.1.90.exe` (Recommended — one-click install)
* 💼 **Portable Archive**: `NexVR-Engine-Portable-0.1.90.exe` (Extract and run anywhere without installing)

> 💡 **Antivirus Note**: Because NexVR intercepts graphics pipelines in real time, Windows Defender may show a SmartScreen warning. NexVR binaries are Authenticode signed. You can click **More Info -> Run Anyway**, or run this quick PowerShell command to whitelist the folder:
> ```powershell
> Add-MpPreference -ExclusionPath "$env:LOCALAPPDATA\Programs\launcher"
> ```
> *(Or click "COPY DEFENDER EXCLUSION" inside the Launcher's About tab).*

---

### 🎯 2. Phase 1 Mission: The "Trinity" Titles

For this first phase, our primary focus is verifying stability and visual fidelity across three core benchmark titles:

1. 🥷 **Sekiro: Shadows Die Twice (DirectX 11)**
   * **What to check**: True 1:1 color contrast (no washed-out gray fog or lifted shadows), responsive head tracking during fast combat.
2. ⚔️ **Mortal Shell (DirectX 11 / Unreal Engine 4)**
   * **What to check**: Flawless 3D depth unprojection (Reverse-Z), rock-solid camera lock even when stationary or walking with WASD (no 3D/2D flickers).
3. 💍 **Elden Ring (DirectX 12)**
   * **What to check**: Smooth 6DOF stereo depth in FromSoftware's DX12 engine.
   * ⚠️ *Important*: **Must be launched in Offline Mode with Anti-Cheat (EAC) disabled.**

*(Have other games? You can also test Cyberpunk 2077, Hogwarts Legacy, or any of the 14 auto-detected profiles).*

---

### ⚙️ 3. Recommended In-Game Graphics Settings

Before launching each game from the NexVR Launcher:
* **Display Mode**: **Borderless Windowed** (Required for seamless injection).
* **Resolution**: Native 16:9 (1920×1080 or 2560×1440).
* **In-Game VSync**: **OFF** (NexVR automatically syncs to your headset's 90 Hz / 120 Hz cadence).
* **Motion Blur & Depth of Field**: **OFF** (Prevents smearing and artificial optical blurring in VR).

---

### 🥽 4. In-Headset Dashboard & Adjustments

Once in-game with your headset on:
* **Toggle VR Dashboard**: Press the **Left Controller Menu Button**, **L3 + R3** on your gamepad, or keyboard **HOME**.
* **Real-time Tuning**: Adjust **IPD (eye separation)**, **Stereo Depth**, or **Convergence** to match your personal comfort.
* **Recenter View**: Hold **R3** or use your controller center action.

---

### 📝 5. How to Report Feedback

If you experience any crashes, tracking stutter, or visual glitches:
1. Open the NexVR Launcher.
2. Go to **About** -> Click **"COPY SYSTEM SPECS"**.
3. Paste the specs along with your headset model (Quest Link, Virtual Desktop, Index, etc.) and what you experienced in this channel.

*Thank you for helping us shape the future of universal flat-to-VR gaming!*
```
