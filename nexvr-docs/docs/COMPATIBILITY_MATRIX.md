# NexVR Engine — Commercial Game & Platform Compatibility Matrix

**Document Version:** 1.0.0-beta  
**Last Updated:** August 15, 2026  
**Scope:** Verified PC Games, Graphics APIs, GPU Vendors, Drivers, and OpenXR Runtimes

---

## 1. Verified PC Games Matrix

| Game Title | Engine / Version | Graphics API | Camera Hook Mode | Depth Pipeline | Anti-Cheat Posture | Verified VR Runtimes | Status |
|---|---|---|---|---|---|---|---|
| **Hogwarts Legacy** | Unreal Engine 4.27 | DirectX 12 | UE Native + Universal Scanner | Universal Reversed-Z (FP32) | Tier 1 (Single Player) | SteamVR, Meta Quest Link, Virtual Desktop | 🟢 **Verified Stable** |
| **Sekiro: Shadows Die Twice** | FromSoftware Engine | DirectX 11 | Universal Scanner | Standard Linear (UNORM) | Tier 1 (Single Player) | SteamVR, Meta Quest Link | 🟢 **Verified Stable** |
| **No Man's Sky** | Custom Vulkan Engine | Vulkan 1.2+ | Universal Scanner | Vulkan Reverse-Z Depth | Tier 1 (Offline Mode) | SteamVR, Virtual Desktop | 🟢 **Verified Stable** |
| **Mortal Shell** | Unreal Engine 4.25 | DirectX 11 / DX12 | UE Native Hook | UE Standard Depth | Tier 1 (Single Player) | SteamVR, Meta Quest Link | 🟢 **Verified Stable** |
| **Elden Ring** | FromSoftware Engine | DirectX 12 | Universal Scanner | Standard Linear Depth | Tier 2 (Offline / EAC Disabled) | SteamVR, Virtual Desktop | 🟡 **Supported Offline Only** |
| **Cyberpunk 2077** | REDengine 4 | DirectX 12 | Universal Scanner | Reverse-Z Depth Buffer | Tier 1 (Single Player) | SteamVR, Meta Quest Link | 🟡 **Profile in Testing** |
| **The Witcher 3 (Next-Gen)** | REDengine 3/4 | DirectX 12 / DX11 | Universal Scanner | Reverse-Z Depth Buffer | Tier 1 (Single Player) | SteamVR, Meta Quest Link | 🟡 **Profile in Testing** |
| **Resident Evil Village** | RE Engine | DirectX 12 | Universal Scanner | Standard Linear Depth | Tier 1 (Single Player) | SteamVR, Virtual Desktop | 🟡 **Profile in Testing** |

---

## 2. Hardware & Driver Compatibility

| GPU Vendor | Architecture / Series | Minimum Driver Version | DirectX 11 | DirectX 12 | Vulkan | DirectML AI Acceleration |
|---|---|---|:---:|:---:|:---:|:---:|
| **NVIDIA** | GeForce RTX 4000 Series (Ada) | 535.98+ | 🟢 Supported | 🟢 Supported | 🟢 Supported | 🟢 FP16 Tensor Cores |
| **NVIDIA** | GeForce RTX 3000 Series (Ampere) | 511.79+ | 🟢 Supported | 🟢 Supported | 🟢 Supported | 🟢 FP16 Tensor Cores |
| **NVIDIA** | GeForce RTX 2000 / GTX 1600 (Turing) | 472.12+ | 🟢 Supported | 🟢 Supported | 🟢 Supported | 🟢 DirectML Fallback |
| **AMD** | Radeon RX 7000 Series (RDNA 3) | Adrenalin 23.5.2+ | 🟢 Supported | 🟢 Supported | 🟢 Supported | 🟢 DirectML RDNA3 |
| **AMD** | Radeon RX 6000 Series (RDNA 2) | Adrenalin 22.5.1+ | 🟢 Supported | 🟢 Supported | 🟢 Supported | 🟢 DirectML RDNA2 |
| **Intel** | Arc A770 / A750 (Alchemist) | 31.0.101.4575+ | 🟢 Supported | 🟢 Supported | 🟢 Supported | 🟢 DirectML XMX |

---

## 3. OpenXR Headset & Runtime Support

| VR Headset Model | Runtime Provider | Tracking System | Native Swapchain Formats | Status |
|---|---|---|---|:---:|
| **Meta Quest 3 / Quest Pro / Quest 2** | Meta Quest Link / AirLink | Inside-Out 6DOF | DXGI_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM | 🟢 Supported |
| **Meta Quest (Virtual Desktop)** | Virtual Desktop VDXR | Inside-Out 6DOF | DXGI_FORMAT_R8G8B8A8_UNORM_SRGB | 🟢 Supported |
| **Valve Index / HTC Vive Pro** | SteamVR (OpenXR 1.0) | Lighthouse 6DOF | DXGI_FORMAT_R8G8B8A8_UNORM | 🟢 Supported |
| **Bigscreen Beyond** | SteamVR (OpenXR 1.0) | Lighthouse 6DOF | DXGI_FORMAT_R8G8B8A8_UNORM | 🟢 Supported |
| **HP Reverb G2 / Windows MR** | Windows Mixed Reality Runtime | Inside-Out 6DOF | DXGI_FORMAT_B8G8R8A8_UNORM | 🟢 Supported |

---

## 4. Anti-Cheat Posture Tier Matrix

- **Tier 1: Single-Player / Mod-Friendly (Fully Supported)**  
  Games without invasive anti-cheat mechanisms. NexVR injects freely. Whitelist `vrinject.dll` in local antivirus heuristics.
- **Tier 2: Light Anti-Cheat / Offline Modes (Supported Offline Only)**  
  Games with anti-cheat that must be disabled prior to injection (e.g., Elden Ring with EAC disabled).
- **Tier 3: Competitive Kernel-Level Anti-Cheat (Strictly Blacklisted & Blocked)**  
  Competitive multiplayer titles (BattlEye, EasyAntiCheat, Vanguard, Ricochet). NexVR will automatically detect and abort injection to protect user accounts.
