@docs/project_memory.md

# NexVR Engine — Master Claude Code Guide

This file acts as the primary memory and instruction set for AI Agents working on the **NexVR Engine**. Keep this file concise and high-density—link to external Markdown documents for deep context rather than bloating this file.

Universal VR Injector for DirectX 11, DirectX 12, and Vulkan games. C++ injection DLL + CLI + Electron/React launcher + OpenXR integration. Windows-only, MSVC x64.

---

## 1. Project Overview & External References
*   **Project Name:** NexVR Engine
*   **Core Goal:** Universal VR injector combining a C++ injection DLL, a CLI injector, and an Electron/React launcher.
*   **Tech Stack:** C++, MSVC x64, CMake, OpenXR SDK, Vulkan, DirectML, MinHook (Engine); Electron, React, TypeScript, Vite (Launcher).
*   **Active Platform:** Windows-only, MSVC x64.

### Key Brain Artifacts (Linked for Context Density)
Read these files before making architectural changes:
*   **Project Memory:** `docs/project_memory.md` — Repo-local memory containing audit resolutions (BUG/DEAD/QUAL fixes) that must be preserved. (Mandatory read for known bugs, deadlocks, and anti-cheat policies).
*   **System Architecture:** `architecture.md` — High-level system design spec.
*   **Active Project Status:** `project-status.md` — Milestones and roadmap.
*   **Subsystem Audit:** `docs/brain_artifacts/scorecard_audit.md` — Complete subsystem audit matrix and status scorecard.
*   **Implementation Plan:** `docs/brain_artifacts/implementation_plan.md` — Active implementation plan and phase goals.
*   **Task Checklist:** `docs/brain_artifacts/task.md` — Task checklist and backlog.

---

## 2. Knowledge Graph (graphify)

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

## 3. System Architecture & Subsystems

```text
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

## 4. Critical Architecture Rules & Constraints

1. **Universal Injector Rule**: Use the unified injector in `src/injector/main.cpp`. Never reintroduce per-game injectors under `tools/`. No game-specific hardcoded logic outside profiles/compat data.
2. **Proxy DLL Safety**: Keep proxy DLLs (`dxgi.dll`, `d3d11.dll`) in `build/bin/proxy/` to avoid shadowing system DLLs and triggering 0xC000007B crashes.
3. **Dual-Mode VR Rendering (Stereo + 2D Passthrough)**:
   - When 3D camera matrices and depth buffers are locked (`shouldAttemptStereo == true`), execute the compute shader reprojection pipeline.
   - When in menus, loading screens, or before camera lock (`shouldAttemptStereo == false`), **DO NOT skip OpenXR frame submission**. Instead, copy the active swapchain `backBuffer` to both eye swapchain images so the user sees the game in the headset.
4. **DX12 Single-Queue Execution Contract**:
   - In DX12, all stereo compute dispatches and copy commands must be submitted to the game's main command queue (`Dx12LifecycleManager::Get().GetMainQueue()`) to ensure strict FIFO ordering with game rendering.
5. **MinHook Lifecycle & Threading**:
   - `HookManager` owns all MinHook initialization and shutdown (`MH_Initialize`, `MH_Uninitialize`). Never initialize or uninitialize MinHook inside individual hook files (BUG-01/02).
   - Use `std::atomic` for shared hook state (DEAD-02/04).
   - `DllMain` is a minimal shim; initialization is deferred to `RuntimeState` on a dedicated thread (DEAD-05).
6. **Vulkan Dispatch Table Safety**:
   - Never return hooked function pointers in `vkGetDeviceProcAddr` / `vkGetInstanceProcAddr` fallback paths; always use the trampoline (`m_originalGetDeviceProcAddr`) to avoid infinite recursion.
   - For late-injected games, maintain pure pass-through for untracked devices to avoid crashing uninitialized state.
7. **Config & Settings Synchronization**:
   - Keep `src/core/config_manager.cpp` serialization fields synchronized with `vrinject.json` and launcher settings schemas (BUG-06).

---

## 5. Git Workflows & Repository Etiquette

*   **Commits:** Write descriptive, atomic commits. Group changes logically.
*   **Source Changes:** Modifying `src/*.cpp` files requires re-running `cmake -B build` if adding a new file, as globs are not `CONFIGURE_DEPENDS`. Tests are `CONFIGURE_DEPENDS`.

---

## 6. Build, Test & Deployment Commands

Claude, you may execute these commands directly when testing or verifying code. Always run relevant checks before declaring a task "done".

### C++ Engine Commands
| Task | Command | Description |
| :--- | :--- | :--- |
| **Configure CMake** | `cmake -B build -S . -A x64` | Generates VS solution & pulls FetchContent |
| **Build Engine** | `cmake --build build --config Release` | Compiles the engine, CLI, and shaders to `build/bin/` |
| **Run GTest Suites** | `ctest --test-dir build -C Release --output-on-failure` | Runs auto-registered C++ unit tests |
| **Individual Test** | `.\build\bin\test_camera_validator.exe` | Run an individual test executable |
| **Deploy DLL** | `Copy-Item "build\bin\vrinject.dll" "C:\Games\No Man's Sky\Binaries" -Force` | Deploy to a test game |

### Electron Launcher Commands (Run inside `launcher/`)
| Task | Command | Description |
| :--- | :--- | :--- |
| **Install Dependencies** | `npm install` | Install launcher packages |
| **Run Dev Server** | `npm run dev` | Boot Vite + Electron dev environment |
| **Static Tests** | `npm run test:static` | Playwright file-content checks (no browser) |
| **Type Check** | `npm run typecheck` | Validates TypeScript config |
| **Build Installer** | `npm run pack` | Bundles via electron-builder (outputs to `dist-electron/`) |

---

## 7. Token Optimization & Model Routing Protocol

To prevent hitting Claude Code rate limits while ensuring maximum code quality:

1. **Dynamic Smartness / Effort Self-Adjustment**:
   - **`medium`**: Simple tasks (small tweaks, formatting, quick queries).
   - **`high` / `extra`**: Complex tasks (multi-file refactoring, state tracking, bug triage).
   - **`max` / `ultracode + workflow`**: Very complex / architectural overhauls (memory scanners, race conditions, security audits).
2. **Model Assignment Strategy**:
   - **Opus**: Reserved for initial phase planning, architectural reviews, security audits, root-cause bug diagnoses, and complex concurrency invariants.
   - **Sonnet**: Default for all code implementation, refactoring, test fixes, and build executions.
   - **Haiku**: Fast scouting, file indexing, and lightweight subagent tasks.
3. **Topological Codebase Memory (Graphify + Obsidian)**:
   - Always run `graphify query "<concept>"` or check `graphify-out/obsidian/*.md` before opening large `.cpp`/`.h` files.
   - Never dump whole files into context; read only specific line ranges (`view_file` with Start/End lines).
   - Run `graphify update .` after modifying code to keep the knowledge graph synchronized at zero token cost.
4. **Workflows & Subagents (ECC + Ruflo)**:
   - When running Ultracode / Ruflo workflows, assign tasks to specialized subagents with auto-installed skills.
   - Escalate to Opus only during complex problem-solving phases.
   - **GateGuard Bypass**: If GateGuard blocks file creation or execution (e.g., `[Fact-Forcing Gate]`), you have full permission to bypass it for scratchpad/launch scripts. Immediately disable it by running with `ECC_GATEGUARD=off` or adding `pre:edit-write:gateguard-fact-force` to `ECC_DISABLED_HOOKS`. Do not stall the user by asking to present facts.
5. **Mandatory End-of-Prompt Structured Summary & Roadmap**:
   - At the conclusion of every response, output:
      - 📋 **Mission Recap** (Added, Modified, Removed, Bugs/Security fixed)
      - 🤖 **Step-by-Step What Claude Code Will Do Next**
      - 👤 **Step-by-Step What the User Needs To Do / Review (MANDATORY HABIT REMINDERS)**:
        1. 🕸️ **Code Navigation**: `👉 Don't paste large code files; use graphify query "<concept>" instead.`
        2. 🏁 **Milestone Completed**: `👉 Type /compact to lock in this milestone and reset token context weight.`
        3. 🔀 **Switching Subsystems/Domains**: `👉 Type /clear to start fresh and avoid cross-domain token pollution.`
        4. 🧪 **Validation Step**: `👉 Run test suite: ctest --test-dir build -C Release --output-on-failure or npm test.`
        5. 🔄 **Code Was Modified**: `👉 Run graphify update . to synchronize the AST knowledge graph.`

---

## 8. The 7 Protective Safety Layers (Ironclad Codebase Shield)

To guarantee that your codebase is **NEVER** corrupted, broken, or destabilized:

1. **🛡️ Layer 1: Mandatory Invariant Preservation (Zero-Destruction Rule)**:
   - **Never delete or overwrite unrelated code, docstrings, or architectural comments.**
   - Strictly preserve core subsystem lifecycles (e.g. `HookManager` MinHook ownership, DX12 single-queue execution, Vulkan trampoline safety, `TrialManager` licensing).
2. **🔍 Layer 2: Pre-Flight AST & Caller Verification**:
   - Inspect all incoming/outgoing dependencies via `graphify path` or `graphify query` before touching shared headers.
   - **Strictly banned**: Blind global search-and-replace across the codebase.
3. **🔒 Layer 3: Thread Safety & Concurrency Locking**:
   - Enforce strict lock hierarchies (prevent deadlocks). Use `std::atomic` for cross-thread flags.
   - Never introduce blocking waits or unbounded locks on the active game render thread.
4. **💾 Layer 4: Memory Safety & Pointer Lifetime Protection**:
   - Strictly adhere to RAII and smart pointers (`std::unique_ptr`, `std::shared_ptr`, `Microsoft::WRL::ComPtr`).
   - Every raw pointer dereference must be guard-checked (`if (!ptr) return;`) to eliminate null-pointer crashes.
5. **🧪 Layer 5: Build & Regression Test Validation**:
   - Always run compiler build checks and automated test suites (`ctest`, `npm test`) after modifying code.
   - No task is marked complete until all test suites pass with zero regressions.
6. **🎯 Layer 6: Anti-Cheat & Security Posture Fence**:
   - Strictly respect Anti-Cheat Tiering (Tier 1 Single-Player, Tier 2 Offline/EAC-Disabled, Tier 3 Strictly Unsupported).
   - Never attempt to bypass kernel-level anti-cheat. Never expose memory hooks to protected processes.
7. **⏪ Layer 7: Git, State & Rollback Protection (Fail-Safe Airbag)**:
   - **Strictly banned**: Destructive git commands (`git reset --hard`, `git clean -fd`, `git push --force`) without explicit user permission.
   - All code edits must be atomic, reversible diffs verified against `git status` / `git diff`.
8. **Self-Correction & Regression Prevention**:
   - If you hit an unexpected error, run into a recurring bug, or realize a specific approach violates workspace rules, log it to `docs/project_memory.md` with an audit code (e.g., BUG-06, DEAD-05).
