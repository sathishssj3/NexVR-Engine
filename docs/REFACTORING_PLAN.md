# NexVR Engine — Master Refactoring & Migration Plan (v2.1 Final)

**Author:** Principal Software Architect, Graphics Engineer & VR System Maintainer  
**Status:** Approved for Phased Execution  
**Governing Rule:** *Preserve working VR pipeline first. Improve architecture second. Add new features third.*

---

## 1. Target Architecture & Module Boundaries

The refined target architecture places **AI as an engine rendering subsystem** (`engine/ai/`) and organizes the codebase into single-responsibility modules with explicit dependency boundaries:

```
NexVR/
├── apps/
│   ├── nexvr-launcher/         # Electron + React + Tailwind + Vite desktop app
│   └── nexvr-cli/              # Elevated UAC injection CLI (vr-inject-cli.exe)
│
├── engine/
│   ├── core/                   # Engine runtime state machine, logging, config, diagnostics
│   │   ├── context/            # SubsystemContext dependency container (incremental migration)
│   │   ├── lifecycle/          # RuntimeState, ProcessAttach/Detach orchestration
│   │   ├── configuration/      # Validated VRConfig, ConfigManager
│   │   ├── logging/            # Central structured logger with level filtering
│   │   ├── errors/             # Typed error contexts, SEH shields, crash telemetry
│   │   └── threading/          # Thread synchronization, worker pools, atomics
│   │
│   ├── injection/              # HookManager & API-specific interception detours
│   │   ├── common/             # BaseHook, MinHook transaction orchestration
│   │   ├── dx11/               # DX11 Present / Swapchain hooks, DXGI factory
│   │   ├── dx12/               # DX12 Present / CommandQueue hooks
│   │   └── vulkan/             # Vulkan WSI hooks, Vulkan Layer interception
│   │
│   ├── graphics/               # Hardware graphics backends (IGraphicsBackend)
│   │   ├── common/             # IGraphicsBackend, ResourceStateTracker, GPU profiler
│   │   ├── dx11/               # DX11GraphicsBackend, DX11StereoRenderer
│   │   ├── dx12/               # DX12GraphicsBackend, DX12StereoRenderer, FenceManager
│   │   └── vulkan/             # VulkanGraphicsBackend, VulkanStereoRenderer, DispatchTable
│   │
│   ├── rendering/              # Stereoscopic projection, depth warping, shaders
│   │   ├── orchestration/      # FrameOrchestrator (pure lifecycle coordination)
│   │   ├── composition/        # DualModeCompositor (stereo compute vs 2D passthrough)
│   │   ├── stereo/             # StereoPipeline, EyeMatrixGenerator, StereoParams
│   │   ├── depth/              # DepthReprojector, ReverseZDetector
│   │   ├── reprojection/       # Asynchronous Timewarp (ATW), ASW motion vectors
│   │   └── shaders/            # HLSL/SPIR-V compiled compute shaders & headers
│   │
│   ├── xr/                     # OpenXR subsystem & runtime abstraction
│   │   ├── openxr/             # OpenXRRuntimeManager, OpenXRSwapchainManager
│   │   ├── frame/              # OpenXRFrameSubmitter, Dual-mode 2D/3D compositor
│   │   ├── tracking/           # Headset 6DOF pose extraction, motion predictor
│   │   └── input/              # Controller bindings, XInput/RawInput emulation
│   │
│   ├── camera/                 # Heuristic camera & matrix discovery
│   │   ├── detection/          # CandidateCollector, PageScanner, MemoryScanner
│   │   ├── tracking/           # CameraDeltaTracker, MotionCorrelation
│   │   ├── lock/               # CameraLockManager, CameraValidator, RankingEngine
│   │   └── adapters/           # UnrealEngine adapter, Unity adapter, Universal adapter
│   │
│   ├── ai/                     # AI Pipeline (In-engine neural rendering subsystem)
│   │   ├── inference/          # AiModelLoader, TensorBridge, AiScheduler
│   │   ├── inpainting/         # NeuralInpainter (DirectML + bilateral shader fallback)
│   │   ├── comfort/            # ComfortGuard MLP simulation sickness predictor
│   │   └── backends/           # DirectML backend, GPU bilateral fallback (no CPU fallback)
│   │
│   ├── profiles/               # Versioned game compatibility database
│   │   ├── schema/             # JSON Schema v1.0 definitions & validation
│   │   ├── loader/             # ProfileLoader, ProfileDatabase (hybrid resolution)
│   │   └── database/           # Built-in game profiles (*.json)
│   │
│   └── diagnostics/            # Non-blocking telemetry, health monitors, dashboards
│       ├── telemetry/          # TelemetryCoordinator (metrics, frame timing, stats)
│       └── health/             # RuntimeStateMonitor, OpenXRHealthMonitor
│
├── games/
│   └── profiles/               # Shipped default game compatibility profiles
│
├── cmake/                      # Modular CMake build modules
│   ├── dependencies.cmake
│   ├── compiler_warnings.cmake
│   ├── shaders.cmake
│   └── testing.cmake
│
├── tests/                      # Organized testing pyramid
│   ├── unit/                   # Fast isolated unit tests (math, config, profiles)
│   ├── integration/            # Multi-subsystem tests (DX11/DX12/VK backends, OpenXR)
│   ├── smoke/                  # End-to-end "Golden Path" VR smoke test app
│   └── stress/                 # Stress & benchmark tests (10k frame loops, queue storms)
│
└── docs/                       # Architecture, baselines, and developer guides
```

---

## 2. API Stability Designations

To protect architectural boundaries against churn and unintentional coupling:

### Stable Public Engine SPIs (Strictly Versioned)
- `IGraphicsBackend` — Encapsulates DirectX 11, DirectX 12, and Vulkan native rendering.
- `IXRRuntime` — Encapsulates OpenXR initialization, swapchains, tracking, and submission.
- `IAIBackend` — Encapsulates DirectML neural acceleration with mandatory GPU bilateral fallback.
- `IProfileProvider` — Encapsulates data-driven JSON profile queries and override loading.
- `ICameraProvider` — Encapsulates camera matrix candidate discovery and validation.

### Internal Implementation Details (Private to Subsystems)
- `Dx12LifecycleManager` & `VulkanDispatchTable` (Private to Graphics & Injection).
- `MinHook` detours and trampoline pointers (Private to `injection/`).
- Raw descriptor heaps and fence rings (Private to specific backend implementations).

---

## 3. Architecture Dependency Enforcement Rules

The following dependency flow will be verified and enforced during CI static analysis:

```
Launcher (Electron/React)
   │ (CLI invocation / Shared Config JSON / Local IPC)
   ▼
Public Engine API / CLI
   │
   ▼
RuntimeState / SubsystemContext
   │
   ├──────────────┬──────────────┬──────────────┬──────────────┐
   ▼              ▼              ▼              ▼              ▼
Injection      Graphics       Rendering        XR            Camera
   │              │              │              │              │
   └──────────────┴──────┬───────┴──────────────┴──────────────┘
                         ▼
                   AI Subsystem (DirectML)
```

### Prohibited Dependencies (CI Static Rejection Rules):
1. **Engine $\to$ Launcher**: The C++ engine must NEVER include or depend on Electron/React/Node.js files.
2. **Renderer $\to$ Graphics Internals**: `FrameOrchestrator` and `DualModeCompositor` must interact through `IGraphicsBackend`, never directly including DX12/Vulkan driver headers.
3. **Profiles $\to$ Engine Internals**: Game profiles must remain pure data (JSON), never embedding executable C++ hooks.
4. **AI $\to$ Hook Internals**: AI modules must never directly control low-level hooks or swapchain detours.

---

## 4. Core Refactoring Principles & Constraints

### A. One Phase = One Architectural Boundary
> **Hard Rule:** A refactoring phase must not modify unrelated architectural boundaries. For example, Phase 4 (Graphics Backend Isolation) may modify `engine/graphics/` and `IGraphicsBackend`, but is strictly prohibited from touching camera heuristics, OpenXR session logic, or launcher code.

### B. Change-Budget & Scope Escalation Rule
> **Hard Rule:** If an implementation step requires modifying code outside the declared architectural scope of the active phase, the agent must **STOP immediately**, report the cross-boundary dependency, explain why the current architecture prevents clean isolation, and await review rather than expanding scope automatically.

### C. "No Behavior Change" for Infrastructure Refactoring
> For all infrastructure and extraction phases (Phases 1–6), the architectural organization changes, but the underlying GPU execution, synchronization, and algorithmic behavior **must remain strictly equivalent**.

### D. Git Checkpoint Requirements
> Every phase begins with a clean baseline Git checkpoint and ends with a validated Git checkpoint. Never begin the next phase until the previous phase has passed all verification gates and is recorded as a known-good state.

---

## 5. Incremental Singleton Migration Strategy

To eliminate singleton destruction crashes without introducing big-bang regressions, singletons will be migrated subsystem-by-subsystem via a `SubsystemContext`:

```
Legacy State:
Global Meyer's Singleton (static T& Get()) ──> Unmanaged global lifetime

Target State:
RuntimeState ──> SubsystemContext ──> Injected Subsystem Instance
```

### Order of Migration:
1. **Step A: Logging & Diagnostics** (`Logger`, `DiagnosticContext`)
2. **Step B: Configuration** (`ConfigManager`)
3. **Step C: Graphics Backends** (`IGraphicsBackend`, `GpuProfiler`)
4. **Step D: XR Subsystem** (`OpenXRRuntimeManager`, `OpenXRFrameSubmitter`)
5. **Step E: Camera & Depth Managers** (`CameraLockManager`, `DepthLockManager`)
6. **Step F: Hook Managers** (`HookManager`)

*After each step: Build $\to$ Unit Tests $\to$ Integration Tests $\to$ VR Smoke Test.*

---

## 6. Detailed 13-Phase Migration Roadmap

| Phase | Milestone Name | Key Deliverables & Guardrails |
|:---:|---|---|
| **Phase 0** | **Baseline & Reproducibility** | • Establish reproducible build & test run.<br>• Diagnose 2 test failures (`AiDirectMLTest`, `test_vulkan_stress`).<br>• Fix only genuine product defects; document environment dependencies.<br>• Establish mandatory regression verification suite. |
| **Phase 1** | **Warning Classification & Critical Remediation** | • Classify all compiler warnings into **Critical** (UB, memory, threading, GPU lifetime), **Important** (correctness, API misuse, portability), and **Low Risk** (cosmetic, unused).<br>• Fix HLSL shader signed/unsigned conversions in `stereo_reprojection.hlsl`.<br>• Fix `tonemap.hlsl` isnan compiler warning.<br>• Fix only critical correctness issues; do not alter working rendering behavior. |
| **Phase 2** | **Repository & Build Hygiene** | • Relocate root clutter (`*.onnx`, `*.cso`, `*.log`, `*.zip`, dummy `.cpp`) to `models/`, `build/`, `logs/`.<br>• Modularize `CMakeLists.txt` into `cmake/Dependencies.cmake`, `cmake/Shaders.cmake`, `cmake/CompilerFlags.cmake`, `cmake/Testing.cmake`.<br>• Eliminate stale quarantined test targets. |
| **Phase 3** | **Dependency & Subsystem Context** | • Introduce `SubsystemContext` container.<br>• Incrementally migrate Logging and Config singletons.<br>• Verify deterministic teardown on DLL unload. |
| **Phase 4** | **Graphics Backend Isolation** | • Solidify `IGraphicsBackend` stable public interface.<br>• Encapsulate all API-specific device, queue, and swapchain types.<br>• Eliminate scattered `if (backend == DX11)` branching. |
| **Phase 5** | **FrameCoordinator Decomposition** | • Extract `FrameOrchestrator` (lifecycle coordination only).<br>• Extract `DualModeCompositor` (stereo compute vs 2D passthrough).<br>• Extract `TelemetryCoordinator` (metrics, timing, stats). |
| **Phase 6** | **Camera & Depth Subsystem Isolation** | • Consolidate matrix classification, candidate collection, and delta tracking.<br>• Remove duplicate matrix classifiers.<br>• Guard memory scanner with structured exception handling (`seh_shield.h`). |
| **Phase 7** | **Data-Driven Profile System** | • Implement JSON schema v1.0 and `ProfileLoader` with hybrid storage resolution (`profiles/` + `%LOCALAPPDATA%`).<br>• Remove hardcoded game names from `engine_detector.cpp`.<br>• Ship verified profiles for Hogwarts Legacy, Sekiro, No Man's Sky, Mortal Shell. |
| **Phase 8** | **AI Subsystem Encapsulation** | • Isolate AI into `engine/ai/`.<br>• Abstract `NeuralInpainter` and `ComfortGuard` behind `IAIBackend`.<br>• Enforce DirectML GPU $\to$ GPU Bilateral Shader $\to$ 2D fallback (never CPU AI in VR frame loop). |
| **Phase 9** | **OpenXR Lifecycle Refinement** | • *Measure first*: Benchmark session initialization hitch duration.<br>• Determine thread-safe non-blocking points without violating OpenXR requirements.<br>• Preserve deterministic shutdown and robust reconnection. |
| **Phase 10** | **Testing & Compatibility Expansion** | • Implement "Golden Path" End-to-End VR Smoke Test app.<br>• Expand unit & integration tests.<br>• Maintain `docs/COMPATIBILITY_MATRIX.md`. |
| **Phase 11** | **CI/CD & Architecture Enforcement** | • Implement CI dependency boundary checker script.<br>• Automate CodeQL, Cppcheck, and GoogleTest runs on pull requests. |
| **Phase 12** | **Production Packaging & Release** | • Automate code-signing via Microsoft SignTool.<br>• Verify NSIS launcher installer and portable release archive hygiene. |

---

## 7. AI Agent Operating Contract

The AI agent must operate as a controlled implementation agent, not as an autonomous architect:

### Before Modifying Code:
1. Read the relevant architecture documentation and [`docs/GOLDEN_PIPELINE.md`](file:///c:/Users/sathi/.gemini/antigravity/scratch/vr-inject/docs/GOLDEN_PIPELINE.md).
2. Read the current phase requirements and verify the active Git checkpoint.
3. Inspect the affected dependency graph.
4. Identify the minimum set of files required for the phase.
5. State the intended changes clearly before editing.
6. Verify all golden pipeline preservation constraints.

### During Implementation:
1. Modify **only** files required for the current phase (adhere strictly to the one-phase-one-boundary rule).
2. Do not expand scope automatically.
3. Do not rewrite working systems unnecessarily.
4. Do not replace working implementations merely because another implementation appears cleaner.
5. Do not change public interfaces without explicit approval.
6. Do not remove working functionality.
7. Do not suppress compiler warnings merely to achieve a green build.
8. Do not weaken test assertions to make them pass.
9. Do not fabricate test results.
10. Do not modify unrelated subsystems.

### After Implementation:
1. Build (`cmake --build build --config Release`).
2. Run affected unit and integration tests.
3. Run the full regression test suite.
4. Run the VR smoke test.
5. Check performance against baseline thresholds in [`docs/BASELINE.md`](file:///c:/Users/sathi/.gemini/antigravity/scratch/vr-inject/docs/BASELINE.md).
6. Inspect the Git diff.
7. If graphify is available and configured, run `graphify update .`; otherwise document the tool status and continue verification.
8. Update architecture documentation.
9. Report changed files, verification results, and known risks.

> [!CAUTION]
> **Scope Escalation Trigger:** If an unexpected dependency requires changes outside the active phase scope, STOP immediately, report the dependency, and wait for architectural review.
