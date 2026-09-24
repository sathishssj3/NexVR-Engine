# NexVR Engine — Phase 1 (Founder's Ring) Feedback Tracker

Use this document to log, track, and triage incoming feedback from our first 5–10 VIP testers.

---

## 1. Tester Cohort Roster (Phase 1 Target: 5–10 Testers)

| ID | Tester Name / Discord Handle | GPU & Driver Version | Headset & Runtime Setup | Target Games | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **T-01** | *[Tester 1]* | RTX 4090 / 555.xx | Meta Quest 3 (Virtual Desktop) | Sekiro, Mortal Shell | Pending Invite | High priority for 120 Hz test |
| **T-02** | *[Tester 2]* | RTX 3080 / 552.xx | Valve Index (SteamVR) | Sekiro, Elden Ring | Pending Invite | Native SteamVR OpenXR test |
| **T-03** | *[Tester 3]* | RX 7900 XTX / Adrenalin | Meta Quest 3 (Link Cable) | Mortal Shell, Cyberpunk | Pending Invite | AMD Radeon Vulkan / DX12 |
| **T-04** | *[Tester 4]* | RTX 4070 Ti / 555.xx | Bigscreen Beyond (SteamVR) | Sekiro, Hogwarts Legacy | Pending Invite | OLED microdisplay black levels |
| **T-05** | *[Tester 5]* | RTX 3070 / 551.xx | Meta Quest 2 (AirLink) | Sekiro, Mortal Shell | Pending Invite | Mid-tier wireless latency |
| **T-06** | *[Tester 6]* | RTX 4080 / 555.xx | Pimax Crystal (PimaxXR) | Elden Ring, Fallout 4 | Pending Invite | High-FOV / wide aspect test |
| **T-07** | *[Tester 7]* | RTX 3060 / 546.xx | Meta Quest 2 (Virtual Desktop) | Sekiro, Palworld | Pending Invite | Minimum recommended GPU |

---

## 2. Test Verification Matrix

Score each category from **1 (Broken / Unplayable)** to **5 (Pristine / Production Ready)**:

| Tester ID | Game Tested | First Launch Inject (Y/N) | Pacing / FPS (1-5) | Camera Tracking (1-5) | Color & Contrast (1-5) | In-Headset Dashboard (Y/N) | Overall Score (1-10) |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| T-01 | | | | | | | |
| T-02 | | | | | | | |
| T-03 | | | | | | | |
| T-04 | | | | | | | |
| T-05 | | | | | | | |

---

## 3. Discovered Issues & Rapid Triage Log

When an issue is reported, log it here with severity:
* **P0 (Blocker)**: Game crashes on inject, headset disconnect, or total freeze.
* **P1 (Critical)**: Severe head tracking desync, double vision (wrong eye convergence), or black screen.
* **P2 (Major)**: Visual artifact (HUD clipping, minor FOV stretch, or elevated brightness).
* **P3 (Minor)**: Cosmetic UI issue, launcher wording, or hotkey clash.

| Issue ID | Reported By | Game & API | Description / Behavior | Severity | Root Cause Analysis | Fix Status |
| :--- | :--- | :--- | :--- | :---: | :--- | :--- |
| **ISS-01** | | | | | | Open |
| **ISS-02** | | | | | | Open |

---

## 4. Phase 1 Exit Criteria (To Unlock Phase 2)

- [ ] At least 5 distinct testers successfully injected and played $> 30$ minutes continuously.
- [ ] Zero unhandled P0 crashes across NVIDIA and AMD hardware.
- [ ] Confirmed 1:1 color contrast and no washed-out grays in Sekiro on at least 3 headsets.
- [ ] Confirmed zero dropped-frame purple spikes in SteamVR GPU performance graph.
- [ ] In-headset ImGui overlay toggles cleanly with gamepads and controllers.
