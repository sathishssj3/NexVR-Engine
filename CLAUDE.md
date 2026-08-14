# NexVR Engine — Claude Code Guide

Universal VR Injector for DirectX 11, DirectX 12, and Vulkan games. C++ injection DLL + CLI + OpenXR integration. Windows-only, MSVC x64.

---

## 1. Knowledge Graph (graphify)

This repository is fully indexed with **graphify** (`graphify-out/graph.json` contains 9,100+ nodes, 12,600+ edges, and 600+ communities).

### Essential Navigation Commands
- **Query concepts or symbols:** `graphify query "<question>"`
- **Inspect relationships between two files/symbols:** `graphify path "<A>" "<B>"`
- **Deep-dive a specific class or concept:** `graphify explain "<concept>"`
- **List core architectural hub nodes:** `graphify god-nodes`
- **Rebuild after modifying code:** `graphify update .` (AST-only, instant, zero API cost)

> [!IMPORTANT]
> **Always run `graphify update .` after modifying `.cpp` or `.h` files to keep the graph current.**

---

## 2. System Architecture & Subsystems

```
Game Rendering Pipeline (DX11 / DX12 / Vulkan)
       │ (Present / Swapchain Hooks)
       ▼
   HookManager (MinHook lifecycle & Detour management)
       │
       ▼
   Lifecycle Managers (Dx11LifecycleManager, Dx12LifecycleManager, VulkanLifecycleManager)
       │ (Extracts active backbuffers & validates epoch)
       ▼
   FrameCoordinator (Orchestrates VR frame submission)
       ├── 2D Passthrough / Menu Fallback (copies backbuffer to OpenXR swapchain)
       └── 3D Stereoscopic Pipeline (Depth reprojection compute shaders)
             ├── CandidateCollector & CameraLockManager (Matrix discovery)
             ├── DepthCandidateCollector & DepthLockManager (Depth discovery)
             └── Graphics Backend (DX11, DX12, Vulkan Stereo Renderers)
                   │
                   ▼
             OpenXR Subsystem (OpenXRRuntimeManager, OpenXRFrameSubmitter, OpenXRSwapchainManager)
                   │
                   ▼
             VR Headset (SteamVR / Meta Quest / Virtual Desktop)
```

---

## 3. Project Memory & Key Brain Artifacts

Read these files before making architectural changes:
- `docs/project_memory.md` — Repo-local memory containing audit resolutions (BUG/DEAD/QUAL fixes) that must be preserved.
- `docs/brain_artifacts/scorecard_audit.md` — Complete subsystem audit matrix and status scorecard.
- `docs/brain_artifacts/implementation_plan.md` — Active implementation plan and phase goals.
- `docs/brain_artifacts/walkthrough.md` — Record of recent fixes and verification logs.
- `docs/brain_artifacts/task.md` — Task checklist and backlog.

---

## 4. Critical Architecture Rules & Gotchas

1. **Dual-Mode VR Rendering (Stereo + 2D Passthrough)**:
   - When 3D camera matrices and depth buffers are locked (`shouldAttemptStereo == true`), execute the compute shader reprojection pipeline.
   - When in menus, loading screens, or before camera lock (`shouldAttemptStereo == false`), **DO NOT skip OpenXR frame submission**. Instead, copy the active swapchain `backBuffer` to both eye swapchain images so the user sees the game in the headset.
2. **DX12 Single-Queue Execution Contract**:
   - In DX12, all stereo compute dispatches and copy commands must be submitted to the game's main command queue (`Dx12LifecycleManager::Get().GetMainQueue()`) to ensure strict FIFO ordering with game rendering.
3. **MinHook Lifecycle**:
   - `HookManager` owns all MinHook initialization and shutdown (`MH_Initialize`, `MH_Uninitialize`). Never initialize or uninitialize MinHook inside individual hook files (BUG-01/02).
   - Use `std::atomic` for shared hook state (DEAD-02/04).
   - `DllMain` is a minimal shim; initialization is deferred to `RuntimeState` on a dedicated thread (DEAD-05).
4. **Vulkan Dispatch Table Safety**:
   - Never return hooked function pointers in `vkGetDeviceProcAddr` / `vkGetInstanceProcAddr` fallback paths; always use the trampoline (`m_originalGetDeviceProcAddr`) to avoid infinite recursion.
   - For late-injected games, maintain pure pass-through for untracked devices to avoid crashing uninitialized state.
5. **Config & Settings Synchronization**:
   - Keep `src/core/config_manager.cpp` serialization fields synchronized with `vrinject.json` and launcher settings schemas (BUG-06).

---

## 5. Build, Test & Deployment

### Build the C++ Engine (Release x64)
```powershell
# Build vrinject.dll
cmake --build build --config Release --target vrinject

# Or build the full solution (DLL + CLI + Test suites)
cmake --build build --config Release
```

### Deploy DLL to Test Game (e.g. No Man's Sky)
```powershell
Copy-Item "build\bin\vrinject.dll" "C:\Games\No Man's Sky\Binaries" -Force
```

### Run GoogleTest Test Suites
```powershell
ctest --test-dir build -C Release --output-on-failure
# Or run an individual test executable:
.\build\bin\test_camera_validator.exe
```

### Electron/React Launcher (if working on frontend)
```powershell
cd launcher
npm install
npm run dev      # Launch Vite + Electron dev environment
npm run build    # Compile Vite + TypeScript
npm test         # Run all specs
```
