# 📋 Project Intake Brief: NexVR Engine

> 📌 **TL;DR**: 
> - **Product**: 1-click universal PC-to-VR desktop launcher and runtime.
> - **Founder Advantage**: Rare C++, DirectX 11/12, Vulkan, and OpenXR systems engineering background (5/5 score on SeedAngels).
> - **Code Readiness**: Monorepo already built and tested (85 C++ unit test suites, 17 CI tests, signed production installers `v0.1.98`).
> - **Target ICP**: PCVR gamers with Quest 3 / Index and an RTX 3070+ GPU wanting to play AAA campaigns in VR.

---

## 🎯 Executive Overview

* **Product Name**: NexVR Engine
* **The Pitch**: Turn your existing flat PC game library into full 6DOF VR with 1 click, locked 90 FPS, and zero modding headaches.
* **Core Positioning**: The **"Console Experience for VR Conversion"** (like Steam or Discord, not an amateur mod).

---

## 🛠️ The Technical Moat & Founder Fit

* **Founder Skillset**: Low-level systems and graphics engineer capable of intercepting multi-API swapchains, decoding Reverse-Z depth buffers, and writing custom HLSL compute shaders.
* **Working Shipped Code**:
  - Windows C++ native injection core (`vrinject.dll`, `vr-inject-cli.exe`).
  - OpenXR 1.0.34 compositors (`XR_KHR_D3D11_enable`, `XR_KHR_D3D12_enable`).
  - Electron/React launcher with Steam/Epic/EA library scanning and Pre-Launch System Health Doctor.
  - Authenticode code-signing and Ed25519-signed OTA updates.

---

## 💰 Monetization & Target Model

* **Free Community Tier**: Universal heuristic injector and community profile loader.
* **$39 Founder Lifetime Pass**: 10 Verified Hero Masterpieces, bilateral compute filter, cloud profile sync.
* **Target LTV:CAC**: **4.65 : 1** ($39.50 LTV vs $8.50 CAC).
