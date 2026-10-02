# ⚠️ Risk Matrix & Mitigations

> 📌 **TL;DR**: 
> - **Top Risk**: Anti-cheat bans if a user injects into online multiplayer $\rightarrow$ **Solved via hardcoded blacklist tripwire**.
> - **Second Risk**: Steam game patches breaking memory offsets $\rightarrow$ **Solved via driver-level DirectX hooks and cloud profile sync**.
> - **Third Risk**: Motion sickness $\rightarrow$ **Solved via curved floating HUD and dynamic FOV vignettes**.

---

## 🧭 The 5 Core Risks & Shields

### 1. Anti-Cheat Account Bans (FATAL RISK)
* **The Danger**: User tries injecting into *Apex Legends* or *Elden Ring Online* and gets banned.
* **Our Shield**: Hardcoded blacklist preventing injection into games with Easy Anti-Cheat, BattlEye, or Vanguard. Supported games are forced into offline mode.

### 2. Game Updates Breaking Injections (HIGH RISK)
* **The Danger**: A 50MB Steam update drops, crashing the game.
* **Our Shield**: Driver-level swapchain detours decouple hooks from game code. Camera matrices are updated over the air from Cloudflare R2 within hours.

### 3. Motion Sickness & Eye Strain (MEDIUM RISK)
* **The Danger**: Users with low VR tolerance experience nausea.
* **Our Shield**: Curved world-space floating HUD prevents retinal strain; dynamic edge vignette activates during fast rotation.

### 4. Windows Defender False Positives (MEDIUM RISK)
* **The Danger**: Antivirus silently deletes injection DLLs.
* **Our Shield**: Authenticode code-signing on all binaries + 1-click elevated PowerShell Defender whitelist tool in the System Doctor.

### 5. High Refund Rate on Founder Pass (LOW RISK)
* **The Danger**: User with a low-spec GTX 1060 buys the pass and cannot reach 90 FPS.
* **Our Shield**: System Doctor warns users before purchase if their GPU is below the RTX 3070 baseline. 14-day refund guarantee eliminates chargebacks.
