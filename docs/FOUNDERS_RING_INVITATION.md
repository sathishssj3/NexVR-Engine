# NexVR Engine — Closed Beta Invitation Templates

---

## 🎯 The "Developer Co-Creator" 2-Step DM Strategy (Recommended)
*Use this when reaching out to PC VR gamers on Discord or Reddit. It builds 100% trust, eliminates malware suspicion, and gets testers excited to test on their hardware.*

### Message 1: The Initial Hook (Send this first — NO `.exe` link)
```markdown
Hey! 👋 Saw you have a PC VR setup.

I'm an engine developer building **NexVR**—a universal DirectX 11/12 injection runtime for PC games (similar to UEVR, but engine-agnostic so it works on FromSoftware titles like *Sekiro*).

I just finished the OpenXR compositor detours and heuristic camera scanner, and I'm looking for 2 or 3 Quest / PCVR gamers who have *Sekiro* or *Mortal Shell* installed to test run the alpha on real hardware.

Would you be open to test-launching it on your headset and letting me know how the 6DOF tracking and frame pacing feel?
```

---

### Message 2: When They Reply ("Sure / Yeah / Send it")
*Send this only after they say yes. Notice it includes your GitHub link for instant credibility.*

```markdown
Awesome, really appreciate it! 🙌

Here's our open GitHub repo so you can inspect the code and release:
🔗 https://github.com/sathishssj3/NexVR-Engine

📦 **1-Click Installer (v0.1.96)**:
https://github.com/sathishssj3/NexVR-Engine/releases/download/v0.1.96/NexVR-Engine-Setup-0.1.96.exe

🎮 **Quick test steps:**
1. Connect your headset (Quest via Virtual Desktop or Link, Index, etc.).
2. Open NexVR (it auto-detects your Steam games).
3. Set the game to **Borderless Windowed**, then click **Launch in VR**!
4. Press the **Left Controller Menu button** in-game to tweak comfort settings (Curved HUD, Motion Vignette, Horizon Lock, or Stereo Depth).

There is a 1-click **"REPORT ISSUE"** button in the launcher's About tab that sends your log directly to my Discord if anything acts up. Let me know how the stereo depth and camera feel! ⚔️🥽
```

---

## 📋 Full Discord Channel Announcement (#beta-testing)
*Use this as a pinned post in your Discord `#announcements` or `#beta-testing` channel.*

```markdown
👋 **Welcome to the NexVR Engine Closed Beta (Founder's Ring / Phase 1)!**

Thank you for being one of our first hand-picked testers. You are receiving early access to **NexVR Engine v0.1.96-beta**, a universal injection engine that converts standard flat PC games into immersive, stereoscopic 3D VR experiences with real-time camera tracking.

---

### 🚀 1. Download & Installation
* 📦 **Standard Installer (Recommended)**: [Download NexVR-Engine-Setup-0.1.96.exe](https://github.com/sathishssj3/NexVR-Engine/releases/download/v0.1.96/NexVR-Engine-Setup-0.1.96.exe)
* 💼 **Standalone Portable**: [Download NexVR-Engine-Portable-0.1.96.exe](https://github.com/sathishssj3/NexVR-Engine/releases/download/v0.1.96/NexVR-Engine-Portable-0.1.96.exe)

> 💡 **Antivirus Note**: Because NexVR intercepts graphics pipelines in real time, Windows Defender may show a SmartScreen warning. NexVR binaries are Authenticode signed. Click **More Info -> Run Anyway**.

---

### 🎯 2. Phase 1 Focus Titles
1. 🥷 **Sekiro: Shadows Die Twice (DirectX 11)** — High-speed combat & fast head tracking.
2. ⚔️ **Mortal Shell (DirectX 11 / Unreal Engine 4)** — Deep 3D reverse-Z depth unprojection.
3. 💍 **Elden Ring (DirectX 12)** — Offline mode only with anti-cheat disabled.

---

### ⚙️ 3. Recommended In-Game Settings
* **Display Mode**: **Borderless Windowed** (Required).
* **Resolution**: 1920×1080 or 2560×1440.
* **In-Game VSync**: **OFF** (NexVR automatically syncs to your headset's 90Hz/120Hz).
* **Motion Blur**: **OFF**.

---

### 🥽 4. In-Headset VR Dashboard
* **Toggle Dashboard**: Press **Left Controller Menu Button**, **L3 + R3** on gamepad, or keyboard **HOME**.
* **Adjust**: IPD, stereo depth, convergence, or recenter view.

---

### 📝 5. How to Report Feedback
* **1-Click in Launcher**: Click **"REPORT ISSUE"** in the About tab. It automatically attaches sanitized logs and dispatches them directly to the engineering team!
* **Or post in Discord**: Drop your notes in `#nexvr-logs` or `#beta-feedback`.
```
