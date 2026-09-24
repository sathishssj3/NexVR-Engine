# NexVR Engine: Startup Strategy & Business Roadmap
*Strategic Planning Document · September 2026 · From Functional Beta to Commercial Launch*

---

## Executive Thesis: Prove Reliability Before Universality

> **Core Strategic Decision:**
> NexVR's immediate challenge is not supporting thousands of games. It is proving that a new user can select a supported game, enter VR, play comfortably for an extended session, and return to do it again—without breaking their game installation or risking an account ban.
> 
> Treat NexVR as a **compatibility and reliability platform first**, a **universal VR conversion platform second**. Launch with a carefully tested catalog, establish trust with PC VR enthusiasts, and expand compatibility only when each additional game can be supported without compromising the experience of existing users.

---

## 12-Month Executive Roadmap

```mermaid
gantt
    title NexVR 12-Month Commercial Execution Roadmap
    dateFormat  YYYY-MM
    section Phase 1: Foundation
    M1-2: Reliability & Safety (Trinity Freeze)     :active, p1, 2026-09, 2026-11
    section Phase 2: Community Beta
    M3-4: Closed Beta (50-100 Testers)              :p2, 2026-11, 2027-01
    section Phase 3: Monetization
    M5-6: Paid Validation ($5/mo & $39/yr)          :p3, 2027-01, 2027-03
    section Phase 4: Public Launch
    M7-9: Public v1.0 & Steam Frame Outreach        :p4, 2027-03, 2027-06
    section Phase 5: Scale
    M10-12: Non-Unreal Catalog Expansion & Angels   :p5, 2027-06, 2027-09
```

### Detailed Phase Breakdown

1. **Months 1–2: Reliability & Safety (Critical Path)**
   - Freeze the initial Trinity beachhead (*Sekiro: Shadows Die Twice*, *Hogwarts Legacy*, *Mortal Shell*).
   - Recognize that *Sekiro* is our sole non-Unreal proof; establish benchmark stability across all 3.
   - Enforce fail-closed anti-cheat detection and remote profile kill-switch.
2. **Months 3–4: Closed Beta (50–100 Qualified Testers)**
   - Measure successful first sessions, crash frequencies, vestibular discomfort, repeat usage, and willingness to pay.
   - Add telemetry tag prompt (comfortable / judder / edge smearing / UI).
3. **Months 5–6: Paid Validation (Priced "Plus" Tier)**
   - Offer an optional $39/year (or $5/mo) Plus subscription. Validate actual purchases and renewal intent.
   - Guardrail: Core injector and all profiles remain 100% free. Paid tier covers only multi-PC cloud sync, GPU auto-tuning, and early engine builds.
4. **Months 7–9: Public v1.0 Commercial Launch (Timed to Steam Frame)**
   - Align launch with Valve Steam Frame hardware shipments and holiday PC VR demand.
   - Initiate creator-led distribution (*Habie147*, *Beardo Benjo*, *VROasis*) featuring *Sekiro* as the non-Unreal flagship.
5. **Months 10–12: Expansion & Angel Round ($100k–$300k)**
   - Add non-Unreal titles (e.g. Unity or FromSoftware titles) to widen the moat against UEVR.
   - Prepare technical evidence pack and open conversations with gaming/tools investors.

---

## 1. MVP Roadmap & Beachhead Strategy

### Current State & Problem
NexVR v0.1.90-beta includes three technically diverse titles in its initial beachhead:

| Game | Graphics API | Engine Type | Role in Beachhead & Moat |
| :--- | :--- | :--- | :--- |
| **Sekiro: Shadows Die Twice** | DirectX 11 | **FromSoftware Proprietary** | **Flagship Moat**: Non-Unreal title where UEVR cannot operate. High-speed combat & camera rotation. |
| **Hogwarts Legacy** | DirectX 12 | Unreal Engine 4/5 | Demanding modern D3D12 pipeline, complex lighting, dynamic shadows. |
| **Mortal Shell** | DirectX 11 | Unreal Engine 4 | Action RPG test for DX11 post-processing and camera delta retention. |

> [!IMPORTANT]
> **The Moat Reality Check:**
> Because *Hogwarts Legacy* and *Mortal Shell* are built on Unreal Engine 4 (where UEVR already has community profiles), **Sekiro is currently our single proof of the engine-agnostic moat**. The next game integrations added to the Verified list must be non-Unreal titles to solidify this advantage.

### v1.0 Acceptance Release Gates

| Metric | Release Target | Operational Significance |
| :--- | :--- | :--- |
| **Successful Launch Rate** | ≥ 95% | On documented supported hardware configurations |
| **First-Time Setup Success** | ≥ 90% | Without manual developer intervention |
| **Crash-Free Sessions** | ≥ 99% | Zero unhandled exceptions caused by NexVR runtime |
| **Defect-Free Sessions** | ≥ 95% | No severe visual glitches, FOV warp, or camera drift |
| **Time to First VR Gameplay** | ≤ 10 minutes | From download completion to in-headset play |
| **Recovery from Failed Launch** | ≤ 2 minutes | Restores game directory cleanly with zero data loss |
| **Anti-Cheat Incidents** | **0** | Strict adherence to single-player offline policy |

---

## 2. Market Validation & Demand Metrics

### Realistic Market Sizing
* **Active Headsets on Steam**: **~2.0M–2.8M** monthly active PC VR headsets on Steam (~1.7% of total Steam userbase).
* **The Tailwinds**: **Valve Steam Frame** (announced Sept 2026 at $1,059 / $1,299) designed to stream flat PC games into spatial environments.
* **The Software Gap**: Over **95% of the top 500 flat PC blockbusters** lack VR support.

### Key Quantitative Targets

| Metric | Initial Target | Business Purpose |
| :--- | :--- | :--- |
| **Beta Activation** | ≥ 60% of invited testers | Validates setup flow and onboarding appeal |
| **First-Session Success** | ≥ 90% of activated testers | Tests out-of-the-box hardware compatibility |
| **D7 Retention** | ≥ 35% | Tests whether users return after novelty fades |
| **D28 Retention** | ≥ 20% | Demonstrates long-term utility |
| **Median Session Duration** | ≥ 40 minutes | Assesses ergonomic comfort and immersive value |
| **Paid Conversion** | ≥ 2–3% of activated users | Realistic benchmark for consumer utility tooling |
| **Refund Rate** | ≤ 10% | Validates that expectations match reality |
| **AV / SmartScreen Blocks** | ≤ 5% of installs | Tests binary reputation and signing trust |

---

## 3. Technical & Technological Requirements

### The Code Signing & Anti-Virus Reality (India Founder Context)
* **SmartScreen Reputation**: Microsoft SmartScreen calculates trust per file SHA-256 hash over time. 
* **The Architecture Rule**: Keep the injector binary (`vr-inject-cli.exe`) **tiny, stable, and rarely recompiled**. Constantly modifying the CLI executable resets its hash reputation with Windows Defender. All frequent updates must land in the user-mode launcher or `vrinject.dll`.
* **Binary Hygiene**: Compile with `asInvoker` by default; elevate only via launcher UAC when targeting an elevated game. Avoid third-party packers or obfuscators that trigger false-positive heuristic flags.

---

## 4. Go-to-Market Strategy & Creator Seeding

### Positioning: Transparency over Hype
> **"Play supported PC games in VR, with a 1-click launcher and verified compatibility."**
> *(Never claim 90,000 games work until individual profiles are tested and verified).*

### Creator Seeding
* **Priority Creators**: Focus outreach on *Beardo Benjo*, *Habie147*, and *VROasis*.
* **The Demo Reel**: Pitch with a **30-second side-by-side clip of *Sekiro* in 6DOF VR** (a non-Unreal title they have never seen running in native 6DOF VR).
* **Timing**: Coordinate video coverage with the delivery of the **Valve Steam Frame**.

---

## 5. Business Model & DMCA Insulation

### The Luke Ross DMCA Precedent: Why Profiles Must Be Free
In January 2026, **CD Projekt RED hit Luke Ross with a DMCA takedown** for paywalling his *Cyberpunk 2077* VR mod on Patreon. Take-Two and 505 Games executed similar takedowns. Luke Ross was forced to make all his mods **100% free**, maintaining Patreon purely for general supporter early-access.

> [!CAUTION]
> **Legal Red Line:**
> Charging money for access to specific game VR conversions or game profiles is what triggers publisher DMCA takedowns. NexVR will **NEVER** paywall game profiles or the core injector.

### Free vs. Paid "Plus" Tier

| Feature | Free Tier | Plus Tier ($39/year or $5/mo) |
| :--- | :---: | :---: |
| **Core Ingestion Engine (DX11, DX12, Vulkan)** | ✅ 100% Free | ✅ Included |
| **All Game Profiles & Community Profiles** | ✅ 100% Free | ✅ Included |
| **In-Headset VR Dashboard & 6DOF Tracking** | ✅ 100% Free | ✅ Included |
| **Automatic OTA Profile Updates** | ✅ 100% Free | ✅ Included |
| **Multi-PC Cloud Settings Sync & Backup** | ❌ | ✅ Included |
| **Telemetry-Driven GPU Auto-Tuning Presets**| ❌ | ✅ Included |
| **Early-Access Engine Builds (Experimental)** | ❌ | ✅ Included |
| **Neural Inpainting & Advanced Filters** | ❌ | ✅ Included (v2.0) |

---

## 6. Legal, EULA & Anti-Cheat Safeguards

### The 3-Tier Anti-Cheat Posture

```
+-----------------------------------------------------------------+
| TIER 1: Single-Player / Offline (FULLY SUPPORTED)               |
| No active anti-cheat. Single-player titles. Supported natively. |
+-----------------------------------------------------------------+
| TIER 2: Light Anti-Cheat / Mod-Friendly (OFFLINE ONLY)          |
| Games with EAC disabled or launched in offline mode.            |
+-----------------------------------------------------------------+
| TIER 3: Strict Anti-Cheat (PERMANENTLY BLACKLISTED)             |
| Vanguard, BattlEye, active EAC, Ricochet. Injection blocked.    |
+-----------------------------------------------------------------+
```

* **Fail-Closed Protection**: Injected CLI checks for active kernel anti-cheat hooks. If detected, injection immediately aborts.
* **Remote Kill-Switch**: Cloudflare Edge API can instantly disable a game profile if a publisher raises an objection or updates security software.
* **Cover Art Hygiene**: The launcher loads box art dynamically from the user's local Steam cache at runtime—NexVR never bundles or distributes third-party copyrighted game artwork.

---

## 7. Funding & Resource Allocation

### Realistic Sizing: Indie Business vs Venture Scale
* A pure PC VR enthusiast subscription reaches **~$80,000 to $250,000 ARR** as a profitable solo/indie business.
* To reach venture scale ($10M+ ARR), NexVR's long-term expansion thesis is:
  1. **Spatial Hardware Support**: Extending Side-by-Side 3D output to lightweight XR glasses (*XREAL, Rokid, Viture*).
  2. **Steam Frame / SteamOS Layer**: Providing zero-friction flat-to-VR streaming on Valve's new spatial handheld.
  3. **Studio B2B Licensing**: Helping flat game developers release official VR editions with zero code rewrites.

### Target $100k–$150k Angel Check Allocation
1. **60%**: Technical hire (C++/Direct3D/Vulkan systems engineer).
2. **15%**: Legal consultation (IP/gaming EULA specialist and US entity setup).
3. **15%**: Hardware test lab (multi-GPU test rigs, Valve Steam Frame).
4. **10%**: Buffer reserve.
