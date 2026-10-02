# Subsystem: Core Engine (`src/core/`)

The **Core** subsystem is the central orchestration layer of NexVR Engine. It coordinates lifecycle states, per-game configuration, engine detection, thread health, and render frame coordination.

---

## Key Components

| Component | File | Description |
| :--- | :--- | :--- |
| **`RuntimeState`** | `runtime_state.h/.cpp` | Manages DLL lifecycle (`Starting`, `Running`, `Stopping`, `Error`). Spawns the worker thread, initializes the SEH crash shield, and orchestrates subsystem teardown. |
| **`ConfigManager`** | `config_manager.h/.cpp` | Loads and saves `vrinject.json`. Implements hierarchical discovery (checking `moduleDir`, parent directories up to 3 levels, and user AppData) and per-game overrides. |
| **`EngineDetector`** | `engine_detector.h/.cpp` | Detects whether the game runs on Unreal Engine 4, Unreal Engine 5, Unity, or custom engines. Prioritizes declarative profile overrides before falling back to module and layout inspection. |
| **`FrameCoordinator`** | `frame_coordinator.h/.cpp` | Bridges the intercepted render swapchain Present call with OpenXR frame submission, stereo shaders, and AI schedulers. |
| **`SubsystemContext`** | `subsystem_context.h` | Thread-safe service locator providing access to active singletons (Logger, Config, Diagnostics, Scanner). |
| **`Logger`** | `logger.h` | Asynchronous file logger writing to `%LOCALAPPDATA%\VRInject\vrinject.log`. |

---

## Golden Architectural Invariants

1. **Early Config Loading**: `ConfigManager::Load(dllDir)` must execute during `RuntimeState::WorkerThread` before any graphics hooks are enabled.
2. **Profile-First Tuning**: Never introduce hardcoded game title checks into `EngineDetector`. Custom matrix or depth settings belong in declarative JSON profiles.
3. **Non-Blocking Render Thread**: Heavy tasks (such as OpenXR instance creation or signature scanning) must run on worker threads, never blocking `FrameCoordinator::OnPresentBegin`.
