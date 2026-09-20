# NexVR Engine — Closed Beta Testing Plan & Runbook
**Version:** 0.1.78-beta  
**Classification:** Confidential — Core Team & Beta Operations

---

## 1. Objectives of the Closed Beta

The primary goals of the NexVR Engine Closed Beta are:
1. **Real-World Hardware Validation:** Verify native OpenXR stereoscopic injection across diverse consumer hardware (NVIDIA RTX 20/30/40 series, AMD Radeon RX 6000/7000 series, Intel Arc).
2. **Headset & Runtime Coverage:** Test compatibility with Meta Quest 2 / 3 / Pro (via Quest Link, AirLink, and Virtual Desktop), Valve Index, Bigscreen Beyond, and HTC Vive via SteamVR.
3. **Graphics API Stability:** Verify zero-crash injection across DirectX 11, DirectX 12, and Vulkan titles.
4. **UX & Onboarding Flow:** Validate the one-click installer, library auto-detection, Windows Defender exclusion flow, and diagnostic reporting.
5. **Security & Code Protection:** Ensure proprietary binaries are distributed securely with zero source leaks.

---

## 2. Phased Rollout Schedule

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│     Phase 0     │ ──► │     Phase 1     │ ──► │     Phase 2     │ ──► │     Phase 3     │
│  Pre-Flight &   │     │  Founder's Ring │     │   Beta Wave 1   │     │   Beta Wave 2   │
│   Packaging     │     │ (5-10 Testers)  │     │ (30-50 Testers) │     │(100-250 Testers)│
│    [Week 0]     │     │    [Week 1]     │     │   [Weeks 2-3]   │     │   [Weeks 4-5]   │
└─────────────────┘     └─────────────────┘     └─────────────────┘     └─────────────────┘
```

### Phase 0: Pre-Flight & Build Hardening (Week 0)
*   **Tasks:**
    *   Compile clean Release binaries: `vrinject.dll`, `vr-inject-cli.exe`.
    *   Strip debug symbols (`.pdb`) from shipped DLLs to safeguard proprietary intellectual property.
    *   Build the official NSIS installer: `NexVR-Engine-Setup-0.1.78.exe`.
    *   Run static regression suite (`npm run test:ci`) and verify zero security vulnerabilities.
    *   Verify Windows Defender whitelist instructions work cleanly on vanilla Windows 10 and 11 machines.
*   **Gate Criteria:** Clean build, zero test failures, verified installer execution on a non-dev test machine.

---

### Phase 1: Founder's Ring / VIP Alpha (5–10 Testers, Week 1)
*   **Audience:** Internal core team, trusted friends, and high-spec VR enthusiasts.
*   **Hardware Target:** RTX 3080/4080/4090 + Meta Quest 3 and Valve Index.
*   **Focus Games (The "Trinity"):**
    1.  *Sekiro: Shadows Die Twice* (DirectX 11)
    2.  *Elden Ring* (DirectX 12)
    3.  *No Man's Sky* (Vulkan)
*   **Key Metrics:**
    *   Zero crash-on-inject rate ($> 95\%$).
    *   OpenXR session initialization under 1.5 seconds.
    *   Monoscopic fallback activates gracefully if stereo heuristic is lost.
*   **Channel:** Private `#founders-beta` Discord channel.

---

### Phase 2: Closed Beta Wave 1 (30–50 Testers, Weeks 2–3)
*   **Audience:** Hand-selected community applicants via Discord application form.
*   **Hardware Target:** Mid-tier GPUs (RTX 2060, RTX 3060, RX 6700 XT) and wireless setups (Virtual Desktop / AirLink).
*   **Expanded Scope:**
    *   All 12 Curated Game Profiles (*Cyberpunk 2077*, *Palworld*, *Mortal Shell*, *Atomic Heart*, *Fallout 4*, *Skyrim SE*, *Valheim*, *Hogwarts Legacy*).
    *   User adjustment of IPD, convergence, and world scale.
    *   OTA update deployment: Validate that launcher receives and patches hotfixes via manifest update.
*   **Key Metrics:**
    *   Session durability: Continuous play for $> 60$ minutes without memory leaks or crash.
    *   Framerate pacing: 72/90/120Hz sync stability.

---

### Phase 3: Closed Beta Wave 2 — Scale & Edge Cases (100–250 Testers, Weeks 4–5)
*   **Audience:** Broader enthusiast ring (flight sim, RPG, racing, and shooter players).
*   **Focus:**
    *   "Add Custom Game" feature: Testing unprofiled DirectX/Vulkan titles.
    *   Edge-case third-party launchers (Xbox Game Pass App, EA App, Ubisoft Connect).
    *   Stress testing automated crash telemetry and log reporting.
*   **Key Metrics:**
    *   Crash reporting accuracy ($> 90\%$ actionable logs with system specs).
    *   Under 5% user-reported injection friction.

---

### Phase 4: Release Candidate (RC) & Readiness Review (Week 6)
*   **Tasks:**
    *   Feature freeze.
    *   Triage and patch all P0/P1 bugs.
    *   Finalize performance benchmarks and documentation.
    *   Transition to Public Open Beta or Commercial Launch.

---

## 3. Tester Onboarding & Runbook

Every selected beta tester will receive a private invitation containing the following steps:

### Prerequisites for Testers
*   **Operating System:** Windows 10 (21H2+) or Windows 11 64-bit.
*   **Minimum GPU:** NVIDIA GeForce GTX 1660 Super / RTX 2060 or AMD Radeon RX 5600 XT.
*   **Recommended GPU:** NVIDIA GeForce RTX 3070 / 4070 or AMD Radeon RX 6800 XT.
*   **Headset:** Meta Quest 2 / 3 / Pro, Valve Index, HTC Vive, Pico 4, Bigscreen Beyond.
*   **Active OpenXR Runtime:** 
    *   For Quest: Meta Quest Link app (Settings -> General -> OpenXR Runtime -> Set as active) OR Virtual Desktop OpenXR.
    *   For SteamVR headsets: SteamVR (Settings -> OpenXR -> Set as active).

### Step-by-Step Installation Runbook
1.  **Download Installer:** Download `NexVR-Engine-Setup-0.1.78.exe` from the private Discord release link.
2.  **Run Setup:** Install NexVR Engine. It installs cleanly into `%LOCALAPPDATA%\Programs\launcher`.
3.  **Antivirus Exclusion (Recommended):**
    *   Open NexVR Engine.
    *   Navigate to the **ABOUT** tab.
    *   Under **SECURITY & ANTIVIRUS NOTICE**, click `COPY DEFENDER EXCLUSION (POWERSHELL)`.
    *   Open Windows PowerShell as Administrator, right-click to paste, and press Enter.
4.  **Connect VR Headset:** Start Quest Link / Virtual Desktop / SteamVR. Ensure the headset displays the VR home environment.
5.  **Launch & Inject:**
    *   Select your game in the NexVR Engine Launcher.
    *   Click `LAUNCH & INJECT VR`.
    *   Put on your headset and experience the game in stereoscopic 3D!

---

## 4. Bug Reporting & Triage Workflow

Testers report issues through a structured workflow to give developers immediate actionable data:

### Bug Classification Matrix
| Severity | Description | Target Turnaround |
| :--- | :--- | :--- |
| **P0 — Blocker** | Game crash on inject, BSOD, permanent black screen in headset. | Fix within 24 hours (Hotfix) |
| **P1 — Critical** | Severe eye desync, camera spinning, major performance drops ($> 50\%$). | Fix within 48–72 hours |
| **P2 — Major** | HUD/menu depth clipping, controller mapping mismatch, profile load failure. | Fix in next beta wave |
| **P3 — Minor** | UI cosmetic glitch, minor text typo, non-breaking cosmetic issue. | Scheduled for Polish sprint |

### What Testers Submit
In the `#beta-bug-reports` channel, testers use this template:
```markdown
**Game Name:** (e.g. Elden Ring)
**Graphics API:** (e.g. DX12)
**Headset & Runtime:** (e.g. Quest 3 via Virtual Desktop OpenXR)
**GPU & Driver:** (e.g. RTX 4080, Driver 560.81)
**Issue Description:** (What happened?)
**Steps to Reproduce:** (1. Started game, 2. Clicked inject, 3. Crash)
**Attached Diagnostics:** Click "COPY SYSTEM SPECS" & "OPEN LOG FOLDER" in the ABOUT tab and attach `vrinject.log`.
```

---

## 5. Security & Intellectual Property Safeguards

To prevent code theft, reverse engineering, or unauthorized distribution:
1.  **No Source Leaks:** The GitHub repository remains strictly private. Only compiled binaries (`.exe`, `.dll`, `.asar`) are packaged into the installer.
2.  **Stripped Binaries:** All debug symbols (`.pdb`) are excluded from release builds.
3.  **Session Handshake:** The CLI injector uses dynamic internal tokens (`NEXVR_AUTH_TOKEN`) so third-party injectors cannot hijack `vrinject.dll`.
4.  **Anti-Cheat Safe Mode:** NexVR strictly prohibits injection into multiplayer anti-cheat games (Easy Anti-Cheat, BattlEye, Ricochet, Vanguard). The injector detects these and aborts to protect user accounts from bans.
5.  **Private Discord Gating:** Beta builds are shared exclusively through role-gated Discord channels with unique build hashes per wave.

---

## 6. Beta Success Criteria (Sign-Off Checklist)

Before concluding Closed Beta and opening the engine to the public:
*   [ ] $\ge 95\%$ successful injection rate across all 12 Curated Hero titles.
*   [ ] Zero unhandled crashes during normal game startup or exit.
*   [ ] Average render thread injection latency $< 1.5\text{ ms}$.
*   [ ] Flawless verification on Meta Quest Link, Virtual Desktop, and SteamVR.
*   [ ] Positive comfort rating ($> 85\%$) from beta testers regarding IPD and stereoscopic convergence.
