# Stereix Engine — Enterprise Commercial Software License & Integration Agreement
**Copyright (c) 2026 Mesmeran Lab. All rights reserved.**

---

### IMPORTANT NOTICE — READ CAREFULLY
This Commercial Software License and Integration Agreement ("Agreement") governs the licensing, integration, and distribution of the Stereix Engine Software Development Kit binaries, libraries, header files, and engine adapters (collectively, the "Software" or "SDK"), including but not limited to `stereix_sdk.dll`, `stereix_unity.dll`, `stereix_unreal.dll`, and associated import headers.

By compiling, linking, integrating, embedding, or distributing the Software within any video game, simulation, virtual reality application, or commercial product (a "Licensed Title"), the entity or developer doing so ("Licensee") agrees to be bound by the terms and conditions of this Agreement.

---

### 1. Grant of Commercial License
Subject to the terms and conditions of this Agreement and payment of applicable commercial licensing fees:
1. **Integration & Linking**: Licensor grants Licensee a non-exclusive, non-transferable, worldwide license to link the Software (statically or dynamically) into the Licensed Title.
2. **Redistribution Rights**: Licensee is granted the right to redistribute the unmodified runtime binaries (`stereix_sdk.dll`, `stereix_unity.dll`, `stereix_unreal.dll`) solely as embedded components of the compiled binary package of the Licensed Title distributed to end users.
3. **No Standalone Redistribution**: Licensee shall not sublicense, sell, lease, or redistribute the SDK headers or libraries on a standalone basis or as part of a competing development SDK.

---

### 2. Platform Purity & Security Warranties
1. **Zero Process-Injection Guarantee**: Licensor warrants that the official SDK binaries (`stereix_sdk.dll`, `stereix_unity.dll`, `stereix_unreal.dll`) contain zero process-injection primitives, zero remote memory allocation routines, and zero runtime API detouring machinery (MinHook/hooking). The SDK operates strictly via native host engine graphics resources provided through sanctioned DirectX 11, DirectX 12, or OpenXR interfaces.
2. **Commercial Anti-Cheat Compliance**: Licensor warrants that all compiled production binaries enforce modern Windows security mitigation standards, including Address Space Layout Randomization (`/DYNAMICBASE`, `/HIGHENTROPYVA`), Hardware Data Execution Prevention (`/NXCOMPAT`), and Control Flow Guard (`/guard:cf`).

---

### 3. Disclaimer of Warranties ("AS IS")
1. **NO IMPLIED WARRANTIES**: TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THE SOFTWARE IS PROVIDED ON AN **"AS IS" AND "AS AVAILABLE"** BASIS, WITH ALL FAULTS AND DEFECTS. LICENSOR DISCLAIMS ALL WARRANTIES OF ANY KIND, WHETHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, INCLUDING WITHOUT LIMITATION WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE, NON-INFRINGEMENT, AND FREEDOM FROM PROGRAM ERRORS.
2. **GRAPHICS DRIVER & THIRD-PARTY RUNTIME DISCLAIMER**: Licensee acknowledges that real-time virtual reality spatial rendering interacts dynamically with third-party operating systems, GPU hardware (NVIDIA, AMD, Intel), graphics drivers, and OpenXR runtime implementations (Meta Quest Link, SteamVR, Windows Mixed Reality, Varjo). Licensor does not warrant that the operation of the Software will be uninterrupted, error-free, compatible with all future hardware revisions or driver updates, or that all rendering defects or driver timeouts (TDR) can or will be corrected.

---

### 4. Strict Limitation of Liability (Damage Waiver & Cap)
1. **EXCLUSION OF CONSEQUENTIAL AND INDIRECT DAMAGES**:
   **UNDER NO CIRCUMSTANCES SHALL MESMERAN LAB, ITS FOUNDERS, DIRECTORS, OFFICERS, EMPLOYEES, AFFILIATES, AGENTS, OR SUPPLIERS BE LIABLE TO LICENSEE, ITS AFFILIATES, OR ANY THIRD PARTY FOR ANY INDIRECT, INCIDENTAL, CONSEQUENTIAL, SPECIAL, EXEMPLARY, PUNITIVE, OR RELIANCE DAMAGES WHATSOEVER.**
   THIS EXCLUSION APPLIES WITHOUT LIMITATION TO:
   - **LOSS OF REVENUE, PROFITS, OR COMMERCIAL OPPORTUNITY;**
   - **GAME SERVER OR SERVICE DOWNTIME;**
   - **PLAYER CHURN, LOSS OF GOODWILL, OR NEGATIVE REVIEWS;**
   - **COST OF RECOVERY, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;**
   - **CORRUPTION, LOSS, OR ALTERATION OF DATA OR GAME STATE;**
   REGARDLESS OF THE LEGAL OR EQUITABLE THEORY (WHETHER IN CONTRACT, TORT INCLUDING NEGLIGENCE, STRICT LIABILITY, PRODUCT LIABILITY, OR OTHERWISE) UPON WHICH THE CLAIM IS BASED, AND EVEN IF LICENSOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

2. **AGGREGATE MONETARY LIABILITY CAP**:
   **TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL LICENSOR'S TOTAL CUMULATIVE AGGREGATE LIABILITY ARISING OUT OF OR IN CONNECTION WITH THIS AGREEMENT, THE SOFTWARE, OR ITS USE, EXCEED THE TOTAL FEES ACTUALLY PAID BY LICENSEE TO LICENSOR UNDER THIS AGREEMENT DURING THE TWELVE (12) MONTHS IMMEDIATELY PRECEDING THE EVENT GIVING RISE TO LIABILITY, OR ONE HUNDRED US DOLLARS ($100.00 USD), WHICHEVER IS GREATER.**

3. **ALLOCATION OF RISK**:
   Licensee acknowledges and agrees that the pricing, terms, and conditions of this Agreement reflect this strict allocation of risk, and that without these limitations of liability, Licensor would not enter into this Agreement or provide the Software.

---

### 5. Indemnification by Licensee
Licensee agrees to defend, indemnify, and hold harmless Licensor, its officers, directors, employees, and agents from and against any and all claims, liabilities, damages, losses, costs, or expenses (including reasonable attorney's fees) arising out of or resulting from:
1. Any modification, reverse engineering, or unauthorized alteration of the Software by Licensee or its agents;
2. Any claims asserted by end users of the Licensed Title regarding gameplay defects, system instability, or hardware interactions;
3. Any combination of the Software with unauthorized third-party software, anti-cheat interceptors, or unapproved runtime injectables.

---

### 6. Zero-Crash Engineering & Fault Recovery
Licensee acknowledges that the Software incorporates architectural Zero-Crash Structured Exception Handling (SEH) trampolines and GPU watchdog timeouts designed to prevent unhandled access violations or driver hangs from terminating the host process. When an unexpected hardware or driver fault occurs:
1. The Software will fail gracefully, record internal diagnostic status, and return a clean failure code (e.g. `STEREIX_ERROR_RUNTIME_FAILURE` or `STEREIX_ERROR_SWAPCHAIN_TIMEOUT`);
2. Licensee's engine must handle non-zero error codes as documented in the [Stereix B2B SDK Integration Guide](docs/B2B_SDK_INTEGRATION_GUIDE.md);
3. Graceful degradation or frame-skipping by the Software shall not constitute a breach of this Agreement.

---

### 7. Governing Law & Dispute Resolution
1. This Agreement shall be governed by and construed in accordance with the laws of Delaware, United States, without regard to its conflict of laws principles.
2. Any controversy, claim, or dispute arising out of or relating to this Agreement shall be settled exclusively by confidential, binding arbitration in accordance with the commercial arbitration rules of the American Arbitration Association (AAA). Neither party shall be entitled to join or consolidate claims by or against other licensees or arbitrate any claim as a class action.

---

*For enterprise licensing inquiries, custom SLA agreements, or source-code escrow requests, contact `enterprise@stereix.io`.*
