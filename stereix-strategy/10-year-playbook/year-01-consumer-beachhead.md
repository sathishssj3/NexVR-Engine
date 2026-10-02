# 📘 Year 1 Operational Playbook (2026 – 2027)
## The 1-Click Consumer Beachhead & Zero-Debt Foundation

> 📌 **Executive Overview**: 
> Modeled after **Google X Moonshots** and **Meta Reality Labs Systems Engineering**, this dossier provides the end-to-end R&D, architectural planning, industrial execution, and go-to-market playbook for Year 1.
> 
> * **The North Star**: Eradicate PCVR modding friction and establish a self-funding consumer beachhead.
> * **The Kill/Pass Metric**: **3,700 Founder Passes sold ($39)** + **Zero Anti-Cheat bans** = **$185,000 Revenue ($102,000 EBITDA)**.
> * **Technology Readiness**: Advance the C++ injection engine from **TRL 7** (Beta) to **TRL 9** (Proven in Mission Operations).
> * **Founder Net Worth**: **$1,470,000** (Retaining 98% equity).

---

## 🔬 1. The Heilmeier R&D Catechism (Google X / DARPA Framework)

| Question | Executive Answer |
| :--- | :--- |
| **1. What are you trying to do?** | Make playing any flat PC masterpiece in 6DOF Virtual Reality as effortless as pressing "Play" on a console—in under 60 seconds with zero manual file tinkering. |
| **2. How is it done today?** | Gamers use legacy tools (VorpX, UEVR, manual DLL mods). Setup takes 45–120 minutes of `.ini` file editing, causes frequent desktop crashes, induces nausea due to flat HUDs, and breaks with every 50MB Steam update. |
| **3. What is new in our approach?** | Automated game library discovery + System Doctor pre-flight diagnostics + universal DirectX/Vulkan driver-level swapchain detours + DirectML client-side AI depth inpainting + a curved 3D world-space floating HUD. |
| **4. Who cares? If successful, what difference will it make?** | 2.5 million PCVR headset owners whose hardware sits idle due to a lack of AAA games get immediate access to *Sekiro*, *Elden Ring*, *Cyberpunk*, and *Hogwarts Legacy* in locked 90 FPS VR. |
| **5. What are the core technical risks?** | (a) Anti-cheat bans in online titles. (b) Swapchain crashes during resolution changes. (c) Motion sickness from latency. |
| **6. How much will it cost?** | Under **$150/month in fixed infrastructure** (Cloudflare R2 zero-egress hosting + Supabase Auth). R&D is 100% self-funded via Founder Pass pre-orders. |
| **7. How long will it take?** | 12 months to reach 3,700 paid customers and $102,000 in net liquid bank reserves. |
| **8. What are the mid-term and final exams?** | Mid-term: "Founder's Ring" pilot (10 testers, $\ge 8$ launch in <60s with 0 crashes). Final exam: 3,700 paying customers and a sub-2% refund rate. |

---

## 🛠️ 2. Deep-Tech R&D & Algorithms (TRL 7 $\rightarrow$ TRL 9)

```
[Graphics Swapchain Hook] ────► [Depth Buffer Extraction] ────► [DirectML Neural Inpaint] ────► [Curved 3D HUD] ────► [OpenXR Submit]
   (MinHook Detours)               (Z-Buffer Unprojection)         (< 1.5ms Tensor Core)         (Floating Mesh)       (90 FPS Stereo)
```

### A. Mathematical Stereoscopic Reprojection
* **Depth Extraction**: Hook `IDXGISwapChain::Present` to capture the backbuffer and depth-stencil surface (`D3D11_BIND_DEPTH_STENCIL` / `D3D12_RESOURCE_STATE_DEPTH_READ`).
* **Unprojection Formula**: Linearize the non-linear reverse-Z depth buffer $Z_{lin} = \frac{f \cdot n}{f - Z_{buf} \cdot (f - n)}$.
* **Stereo Eye Separation**: Synthesize Left/Right eye cameras using virtual inter-pupillary distance (IPD = 64mm) and calculate disparity displacement $\Delta x = \frac{\text{IPD} \cdot f_x}{Z_{lin}}$.

### B. The 11.1ms Sacred Frame Pacing Law
* 90 Hz VR requires delivering each frame within **11.1 milliseconds**. Any overrun causes head-tracking stutter and motion sickness.
* **Render-Thread Sync AI Budget**: Max **1.5 milliseconds** for DirectML depth inference.
* **Background Worker Pool**: Asynchronous texture upscaling and mesh reconstruction run on background worker threads, never blocking `Present()`.

### C. Anti-Cheat Tripwire Scanner
* Inspect Portable Executable (PE) headers and active process modules at launch.
* Blacklist immediately blocks injection if Easy Anti-Cheat (`EasyAntiCheat.exe`), BattlEye (`BEService.exe`), or Riot Vanguard (`vgc.sys`) are detected.
* Supported games (*Elden Ring*, *Armored Core VI*) are decoupled into verified offline sandbox mode.

---

## 📐 3. System Architecture & Planning (RFCs)

* **RFC-001 (Render Pipeline Integration)**: Standardize all graphics hooks behind the abstract `GraphicsBackend` interface (`include/rendering/graphics_backend.h`).
* **RFC-002 (System Doctor Pre-Flight Diagnostics)**: Inspect GPU driver versions, OpenXR runtime registration, and DirectX feature levels prior to game execution.
* **RFC-003 (Curved Floating HUD Mesh)**: Project 2D HUD UI onto a cylindrical parametric mesh in world space at a fixed 2.2-meter focal distance, eliminating retinal strain.
* **RFC-004 (Zero Cloud GPU Compute)**: Eliminate server-side cloud rendering; all neural models run client-side via ONNX Runtime DirectML on the user's RTX GPU.

---

## ⚙️ 4. Industrial Execution & QA (IBM / Google Rigor)

### Automated Test Rig (GoogleTest + CI):
* **85 C++ Unit Test Suites**: Verify MinHook detours, DXGI swapchain recreation, Vulkan image copy (`vkCmdCopyImage`), and memory cleanup.
* **17 CI Workflows**: Enforce static code analysis, zero compiler warnings, and the **Coupling Ratchet** (`scripts/check_coupling.ps1`).
* **Hardware-in-the-Loop (HIL) Bench**: Test injection against clean Windows 11 installs across Meta Quest 3 (Link/Virtual Desktop/AirLink) and Valve Index on RTX 3070 and RTX 4080.

---

## 📢 5. Go-To-Market & Commercialization (The Beachhead)

```mermaid
flowchart LR
    V1["15s Viral Video<br/>(TikTok/Shorts)<br/>'Library to Space'"] 
    --> W1["Landing Page<br/>(System Doctor Scan)<br/>nexvr-engine.pages.dev"] 
    --> P1["$39 Founder Pass<br/>(Limited 50 Slots)<br/>LemonSqueezy/Stripe"]
    --> A1["20% Creator Affiliates<br/>(3 VR YouTubers)<br/>$7.80 Payout/Sale"]
```

* **Step 1: The "Founder's Ring" 50-Seat Pre-Order**: Send personalized invitations (`docs/FOUNDERS_RING_INVITATION.md`) to 50 active modders on Discord.
* **Step 2: The 15-Second Transformation Hook**: Short-form video demonstrating flat desktop *Sekiro* transitioning into full-scale 3D immersion upon clicking "Launch in VR".
* **Step 3: Creator Affiliate Engine**: Partner with 3 mid-tier PCVR YouTubers (15k–50k subscribers) offering a **20% rev-share ($7.80 per sale)**, keeping customer acquisition cost below **$8.50 CAC**.

---

## 📊 6. Year 1 Financials & Gate Review Criteria

| Financial Metric | Year 1 Target | Operational Reality |
| :--- | :---: | :--- |
| **Founder Lifetime Passes Sold ($39)** | 3,700 | $145,000 |
| **Pro Monthly SaaS Subscribers ($9.99/mo)** | 330 | $40,000 |
| **Consolidated Gross Revenue** | **$185,000** | 100% Consumer-Funded |
| **Cost of Goods Sold (COGS)** | ($13,500) | 92.7% Gross Margin |
| **Operating Expenses (Affiliates/Payroll/Hosting)**| ($69,500) | Zero Office / Lean Bootstrap |
| **Operating Profit (EBITDA)** | **$102,000** | **55.1% Net Margin** |
| **Cumulative Bank Reserves** | **$102,000** | 100% Debt-Free Runway |
| **Founder Equity Ownership** | **98%** | Dilution-Free |
| **FOUNDER NET WORTH** | **$1,470,000** | Seed Valuation: $1.5M |

### 🚦 Year 1 Stage-Gate Exit Criteria (To Unlock Year 2):
1. ✅ Minimum 3,000 paid users on the $39 Founder Pass.
2. ✅ Zero verified user account bans due to anti-cheat tripwires.
3. ✅ 85%+ positive feedback on the "Hero 10" games with <2% refund requests.
4. ✅ At least **$90,000 in liquid cash reserves** in the company bank account.
