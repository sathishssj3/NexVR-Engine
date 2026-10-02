# NexVR Engine — Holistic Architecture & Project Health Review

**Reviewer role:** Principal C++ Software Architect
**Review date:** 2026-08-16
**Branch reviewed:** `fix/vulkan-layer-detour-exclusion` (HEAD `12017b7`)
**Scope:** Read-only. No source files were modified.
**Method:** Document review (`docs/project_memory.md`, `docs/BASELINE.md`, `docs/ARCHITECTURE_AUDIT.md`, `docs/GOLDEN_PIPELINE.md`, `docs/REFACTORING_PLAN.md`, `CLAUDE.md`), graphify knowledge-graph traversal (`query`, `explain`, `god-nodes`, `path`), CMake/CI configuration analysis, and targeted source verification of every claim carried forward from prior audits.

> **Note on prior audits.** `docs/ARCHITECTURE_AUDIT.md` and `docs/brain_artifacts/scorecard_audit.md` were treated as *hypotheses to verify*, not as findings. Each material claim below was independently re-confirmed against the current tree; where a prior audit was stale or wrong, that is stated explicitly.

---

## 🏆 Overall Score

# **66 / 100**

*Grade: C+ — Structurally serious engineering with genuinely exceptional low-level safety work, held back by one unresolved God Object, a missing core capability (head tracking), and a widening gap between what the documentation claims and what the code does.*

### Score derivation

The headline number is weighted, not asserted. Each dimension was scored 0–10 against evidence collected during this review.

| # | Dimension | Weight | Score | Contribution | Primary evidence |
|:--|:---|:---:|:---:|:---:|:---|
| 1 | Build & Toolchain | 10% | 9.0 | 9.0 | C++20, `/GS /guard:cf /EHa`, 3 shader compilers (FXC/DXC/GLSLC), Debug+Release CI matrix, clean link of `NexVRCore` |
| 2 | Defensive Programming & Concurrency | 20% | 8.0 | 16.0 | 193 RAII lock sites / 90 mutex-bearing files; documented lock ordering; fail-closed Vulkan handles; 1 raw `new` in 85k LOC |
| 3 | Architecture & Coupling | 20% | 5.0 | 10.0 | `FrameCoordinator` God Object w/ 14 backend branches; 36 Meyer's singletons; duplicated `MatrixClassifier` |
| 4 | Test Posture | 15% | 6.0 | 9.0 | 70/70 registered pass, but 21 of 95 test sources never run; zero end-to-end VR-path coverage |
| 5 | CI/CD Integration | 10% | 7.0 | 7.0 | CodeQL + cppcheck + ctest + JUnit + code signing; but stale failure allowlist and non-gating static analysis |
| 6 | Documentation & Knowledge Systems | 10% | 8.5 | 8.5 | 5 first-class architecture documents, graphify graph, Golden Pipeline contract; undercut by internal contradictions |
| 7 | Repository Hygiene | 5% | 4.0 | 2.0 | 352 MB `.git`; 66 MB ONNX blob tracked; `dummy.cpp` and `compile_commands.json` tracked |
| 8 | Functional Delivery (end-to-end VR) | 10% | 4.0 | 4.0 | Injection + hooking + `xrEndFrame` reached on 3 APIs; stereo output and head tracking not delivered |
| | **Total** | **100%** | | **65.5 → 66** | |

**Trajectory.** The 2026-08-13 evaluation scored this codebase **67/100**. The near-identical score today is misleading in isolation: dimensions 2 and 5 have materially improved (Vulkan dispatch-map race eliminated, CI now actually executes `ctest`), while dimensions 4 and 8 have been *re-scored downward against better evidence* rather than having regressed. The engineering is improving; the measurement has become more honest.

---

## 🟢 High-Quality Engineering

These are not merely "acceptable" — they are above the standard typically found in injection-layer codebases and should be preserved verbatim under the Layer 1 Zero-Destruction Rule.

### 1. Vulkan layer concurrency work is textbook-grade

`src/hooks/vulkan_layer.cpp` is the strongest single file in the repository. Three properties stand out:

- **Lock ordering is documented at the point of acquisition, not in a wiki.** Lines 171–172 carry the comment *"g_deviceToPhysicalDevice is read under g_swapchainMutex in QueuePresent, so it is written under the same lock. Taken after g_dispatchMutex is released, never nested."* Verified: `g_dispatchMutex` closes at line 169 before `g_swapchainMutex` opens at line 174. The two-lock design has no nesting anywhere in the file, so it cannot deadlock by ordering.
- **Re-entrancy hazard was anticipated.** Lines 151–158 resolve all six next-layer function pointers *before* taking the lock, with the reason stated inline: `gdpa()` re-enters the layer chain below, and holding a mutex across that call is a deadlock risk. This is the kind of hazard that is normally discovered by a hang in production, not by design.
- **Untracked handles fail closed.** `vkCreateSwapchainKHR` on an unknown device returns an error and nulls `*pSwapchain` rather than returning `VK_SUCCESS` with an uninitialized handle (lines 190–195). The comment is explicit about why. Read-path lookups use `find()` rather than `operator[]`, so a miss cannot silently insert a null trampoline into a shared map.

### 2. Layer / detour mutual exclusion is a correct architectural invariant

`IsLayerActive()` (declared `vulkan_hook.h:18`, defined `vulkan_layer.cpp:32` with acquire semantics) is consumed at `hook_manager.cpp:85` to suppress *all* D3D and Vulkan MinHook detours when the Vulkan layer path is live. Running both interception mechanisms simultaneously is the classic double-hook heap-corruption failure; encoding the exclusion as a single atomic gate consulted at the one place that installs detours is the right shape.

### 3. Hook installation is transactional

`HookManager` (`src/core/hook_manager.h:20`) exposes a genuine phase machine — `Prepare` → `Validate` → `Install` → `Verify` → `Commit`, with `Rollback` and an `m_rollbackStack` of `std::function` undo actions guarded by a `std::recursive_mutex`. Graphify confirms it is reached only from `RuntimeState::BackgroundInitialize` (`runtime_state.cpp:86`) and torn down only from `BackgroundTeardown` (`runtime_state.cpp:101`) — single owner, no scattered `MH_Initialize` calls. BUG-01/BUG-02 remain correctly closed.

### 4. Memory safety around untrusted game memory

`SafeReadMemory` (`src/core/seh_shield.h:46`) wraps every read of engine-owned memory in a Windows SEH `__try`/`__except` filtered specifically to `EXCEPTION_ACCESS_VIOLATION`, and — critically — **zeroes the destination before returning `false`** so a partially-copied buffer can never be mistaken for a valid matrix candidate. `IsValidMemoryPointer` rejects near-null, above-canonical, and misaligned pointers before any vtable dereference. For a system whose entire premise is reading pointers it does not own, this is the correct posture.

### 5. Smart-pointer discipline in the hot path

Across 85,315 lines of C++ there is exactly **one** raw `new`. `FrameCoordinator` owns every subsystem through `std::unique_ptr` (`IGraphicsBackend`, all four OpenXR managers) or by value. COM resources use `ComPtr` in 25 files. This is materially better than the `ARCHITECTURE_AUDIT.md` finding #34 suggests.

### 6. Documentation as a first-class artifact

Five substantive architecture documents plus a queryable graphify graph (9,100+ nodes) plus a repo-local `project_memory.md` ledger of closed defects. `docs/GOLDEN_PIPELINE.md` in particular is unusual and valuable: freezing the eight-stage pipeline as a *contract* that refactors must not violate is exactly how you protect a working system during architectural migration.

### 7. Commit hygiene

64 commits in 30 days, each with a subject line that states a behavioral change rather than a file list — *"Vulkan: stop detouring the driver's own presentation path"*, *"DX11: submit real eye dimensions instead of a hardcoded 100x100 imageRect"*. This is reviewable history.

---

## 🟡 Technical Debt & Areas for Growth

### T1 — Test posture is narrower than the headline number implies

`docs/BASELINE.md` reports **70/70 passing (100%)**. That is true and is a real achievement. It is also a measurement of a deliberately reduced denominator.

| Category | Count | Detail |
|:---|:---:|:---|
| Test source files present in `tests/` | 95 | |
| Registered and executing under CTest | 70 | |
| **Quarantined (do not compile)** | **7** | `CMakeLists.txt:770–780` |
| **Zero-byte placeholder files** | **12** | `e2e_test_app.cpp`, `test_stereo_pipeline.cpp`, `test_depth_pipeline.cpp`, `test_vr_runtime.cpp`, … |
| Excluded (duplicate `main`) | 2 | `AUTO_GTEST_EXCLUDES` |

**22% of the test corpus never runs.** The quarantine block is well-engineered debt — it logs each skip at configure time with a reason, and the comment states the goal is to drive the list to empty. Four entries are genuine **API drift** (`AICommandQueue` deleted, `CameraDeltaTracker::UpdateDelta` renamed to `UpdateCandidateMotion`, `GraphicsResourceIdentity::creationGeneration` removed, `Dx11ContextSnapshot` removed) — production code moved and the tests were parked rather than migrated. Three are a **toolchain blocker**: gmock 1.14 static-asserts on `__stdcall` COM interfaces, which no amount of test rewriting fixes.

More significant than the count: the twelve zero-byte files name precisely the untested surfaces — stereo pipeline, depth pipeline, temporal pipeline, VR runtime, runtime integration, reconstruction quality. **Every registered test is a unit test. Nothing exercises the assembled VR frame path.** The Golden Pipeline contract has no executable enforcement.

The launcher is thinner still: **2 spec files against 18 TypeScript sources** in `launcher/src` and `launcher/electron`.

### T2 — CI has a stale known-failure allowlist that will mask a real regression

`.github/workflows/release.yml:152` hardcodes:

```pwsh
$known = @('AiDirectMLTest', 'test_vulkan_stress')
```

If only these two fail, the step emits a warning and **`exit 0`** — the job goes green. But `docs/BASELINE.md` §2 documents both as **RESOLVED** (dynamic model-path resolution via `GetModuleFileNameW`; `InitOriginalGetDeviceProcAddr` added to the Vulkan test device setup). The allowlist is now scaffolding around two doors that are no longer broken, and it will silently absorb any future regression in the DirectML inference path or the Vulkan stress harness — two of the highest-risk subsystems in the product.

Related gating gaps in the same workflow:

- **Cppcheck is advisory only** — the step unconditionally ends `exit 0` with a comment deferring to CodeQL as "primary analyzer."
- **Clang-tidy is commented out** entirely. For a C++20 codebase with 226 `reinterpret_cast` sites, clang-tidy is the single highest-value analyzer available and it is disabled.
- CodeQL runs and is the only enforcing analyzer.

### T3 — Documentation has drifted into self-contradiction

`docs/brain_artifacts/scorecard_audit.md` claims **"23 out of 23 registered CTest executables pass"** and rates the suite **10.0/10** on that basis. `docs/BASELINE.md`, dated one day later, reports **70 registered targets**. Both cannot describe the same tree. The scorecard's overall **8.8/10** rating is therefore anchored to a suite three times smaller than the one that exists, and its "Pillar 4: Deterministic CTest Suite 10.0/10" is not a current measurement.

This matters beyond bookkeeping: `CLAUDE.md` instructs future agents to *"Read these files before making architectural changes."* A stale scorecard read as current will systematically overstate readiness.

### T4 — Duplicated matrix classification

Two independent implementations coexist:

- `src/core/matrix_classifier.cpp` (7,193 bytes, modified 2026-07-29)
- `src/ai_matrix_classifier/matrix_classifier.cpp` (2,154 bytes, modified 2026-06-05)

The `ai_matrix_classifier/` copy is two months staler and roughly a third the size. Camera-matrix classification is the heuristic on which the entire 3D path depends; having two answers to "is this a view matrix?" in one binary is a latent correctness fork.

### T5 — Dead and orphaned subsystems

| Symbol | Status | Evidence |
|:---|:---|:---|
| `InputManager` | **Defined, never instantiated** | `src/hooks/input_manager.{h,cpp}` exist; zero construction or `::Get()` sites anywhere in `src/` |
| `GazePredictor` | Roadmap only | Zero occurrences in `src/` |
| `UISynthesizer` | Roadmap only | Zero occurrences in `src/` |
| `OFAVectorRefiner` | Roadmap only | Zero occurrences in `src/` |
| `NeuralInpainter` | Reachable but untrained | Referenced from `stereo_pipeline.{h,cpp}`; per prior investigation the ONNX weights are untrained and the mask polarity is inverted relative to the shader |

`docs/project_memory.md` §5 lists seven planned AI models. Two exist as code (`NeuralInpainter`, `ComfortGuard`), one of which is non-functional; five do not exist. The roadmap should be labeled as a roadmap in the memory document so it is not mistaken for an inventory.

### T6 — Repository hygiene

- `.git` is **352 MB** for 7,378 tracked files.
- `depth_inpainter.onnx.data` — **66 MB** — is **tracked in git**. Model weights in version control are the usual cause of a `.git` this size.
- `dummy.cpp`, `compile_commands.json`, and `openvr_capi.h` are tracked; the first two are build detritus.
- The working directory additionally holds `HogwartsLegacy.exe` (83 MB), `dxc.zip` (25 MB), `onnx_dml.zip` (19 MB), and `NexVR_Beta_v1.0.zip` (4 MB). **These are correctly untracked/ignored** — prior audit finding #19 overstated this. The tracked 66 MB blob is the real problem.
- `docs/ARCHITECTURE_AUDIT.md` finding #24 (hardcoded `L"../models/depth_inpainter.onnx"`) is **resolved** — a full scan of `src/` finds exactly one absolute-path string literal, and it is inside a comment in `logger.h`. Production source is clean of hardcoded paths.

### T7 — Per-game special-casing in core

`src/core/engine_detector.cpp:83` still contains:

```cpp
if (exeStr.find("HogwartsLegacy") != std::string::npos) {
    LOG_INFO("Detected Hogwarts Legacy - Forcing Unreal Engine mode.");
```

`docs/project_memory.md` §6 explicitly forbids this: *"Avoid hardcoded per-game behavior unless it is isolated behind profiles or compatibility data."* One title today is one title; the pattern is what compounds. This belongs in a versioned JSON compatibility profile, which the refactoring plan already scopes.

### T8 — Blanket exception swallowing on the discovery path

Five `catch (...)` sites remain, four of them on the render-critical path:

| Location | Risk |
|:---|:---|
| `src/core/frame_coordinator.cpp:147` | Wraps camera/depth discovery — an access violation here is indistinguishable from "no candidate found" |
| `src/hooks/dx11_hook.cpp:117` | Present-path interception |
| `src/hooks/dx12_hook.cpp:210`, `:292` | Present-path interception |
| `src/core/base_hook.h:213` | Shared hook base |

Catching everything at a hook boundary is defensible — you must not propagate an exception into the host's render loop. Catching everything and **logging nothing diagnostic** turns a hard memory bug into an intermittent "candidate count = 0", which is exactly the symptom recorded during the 2026-08-16 probe session (7,987 frames, 0 candidates). These handlers should classify and log before swallowing.

---

## 🔴 Immediate Architectural Risks

### R1 — CRITICAL: The VEH shield logs a safety guarantee it does not provide

`RegisterVehShield` **is** correctly installed (`runtime_state.cpp:68`) and unregistered (`:102`) — the prior session note that it was "never installed" was wrong, and is corrected here.

The real defect is worse. `VrinjectVehHandler` (`src/core/seh_shield.h:66–88`) returns `EXCEPTION_CONTINUE_SEARCH` on **every** code path. It never returns `EXCEPTION_CONTINUE_EXECUTION`, never adjusts the context record, never unwinds. It is a pure observer. Yet on the in-module branch it logs:

```
[VEH SHIELD] Intercepted exception 0x%08X inside vrinject.dll at %p. Host process crash prevented!
```

**No crash is prevented.** The exception continues to the next handler exactly as it would with no shield installed. The inner comment on line 82 — *"catch and safely ignore or return execution"* — describes an intent the code does not implement, and line 83's `return EXCEPTION_CONTINUE_SEARCH` contradicts it directly.

Why this is critical rather than cosmetic: a false safety signal in the log is worse than no shield, because triage reads the log. Any future crash investigation that sees "Host process crash prevented!" will conclude the shield worked and look elsewhere. Either implement real continuation semantics or downgrade the message to `[VEH OBSERVER] Exception 0x%08X observed inside vrinject.dll at %p (not handled).`

### R2 — CRITICAL: Head tracking does not exist

`xrLocateViews` appears in the codebase **only** inside the vendored SDK header `src/rendering/openxr/openxr.h` (typedef at :1729, prototype at :1924). There is **not one call site** in NexVR code. The same is true of `xrLocateSpace`.

A VR injector that never queries head pose cannot render a view that responds to the user's head. Whatever is currently reaching `xrEndFrame` is a fixed-pose image in a headset. This is not a defect in the VR pipeline — it is a missing stage of it, and it is the single largest gap between the product's premise and its implementation.

`docs/BASELINE.md` §3 marks **"OpenXR Frame Submission ✅"** in the regression matrix. That checkmark is accurate about frame *submission* and silently misleading about VR *function*. The matrix needs a "Head Pose Acquisition" row, currently ❌.

### R3 — HIGH: `FrameCoordinator` remains a God Object with runtime API branching

The class holds `std::unique_ptr<IGraphicsBackend>` — so the polymorphic seam **exists** — and yet `frame_coordinator.cpp` still contains **14 explicit backend comparisons** on the frame path:

```
:43, :57, :60, :125, :158, :161, :164, :175, :202, :219, :274, :300, :343, :399
```

Each is a `GraphicsBackend::DX11 / DX12 / Vulkan` discriminator. Line 399 is the most telling: `if (m_graphicsBackend && m_currentSnapshot.backend == GraphicsBackend::DX11)` — the code holds the polymorphic handle *and then re-checks the concrete type before using it*.

Concurrently the class owns eight distinct responsibilities: engine detection, CPU profiling, GPU profiling, camera discovery, depth discovery, compatibility scoring, OpenXR lifecycle across four managers, and dashboard rendering. Graphify shows it as the convergence point for all three hooks (`dx11_hook.cpp`, `dx12_hook.cpp`, `vulkan_hook.cpp`, `vulkan_layer.cpp` all include `frame_coordinator.h`).

Consequence: **adding a fourth backend, or changing any one backend's profiling, requires editing the class that every backend routes through.** Every such edit is a change to the hot render path shared by all three APIs — the highest-blast-radius file in the system.

### R4 — HIGH: 36 uncoordinated singletons with undefined destruction order

Thirty-six `static T& Get()` Meyer's singletons across `src/core/`, `src/hooks/`, and `src/rendering/`. Verified list includes `HookManager`, `FrameCoordinator`, `RuntimeState`, `CameraLockManager`, `DepthLockManager`, `DiagnosticContext`, `VulkanDispatchTable`, `VulkanLifecycleManager`, and fourteen further Vulkan trackers.

Meyer's singletons destruct in **reverse order of first construction** — an order determined by runtime call sequence, which varies per game, per injection timing, and per API. During `DLL_PROCESS_DETACH` this means teardown order is effectively nondeterministic. `VulkanDispatchTable` may be destroyed while `VulkanLifecycleManager` still holds handles that route through it.

This risk is compounded by R5.

### R5 — HIGH: Detached teardown thread races DLL unload

`src/core/runtime_state.cpp:34–38`:

```cpp
void RuntimeState::OnDllProcessDetach() {
    TransitionTo(RuntimePhase::Stopping);
    std::thread teardownThread(&RuntimeState::BackgroundTeardown, this);
    teardownThread.detach();
}
```

Deferring work off `DllMain` is correct — it is the documented fix for DEAD-05 loader-lock deadlock, and `DisableThreadLibraryCalls` plus the module pin in `BackgroundInitialize` show the hazard was understood. But `DLL_PROCESS_DETACH` returns immediately after spawning, and **nothing blocks the unload**. If the host is exiting, `BackgroundTeardown` — which calls into `HookManager` (`:101`) to uninstall MinHook detours and then into `UnregisterVehShield` (`:102`) — can be executing inside code pages that the loader is concurrently unmapping.

The failure mode is a crash *on game exit*, attributed to the game, hard to reproduce, and indistinguishable from an unrelated shutdown bug. `docs/project_memory.md` DEAD-05 specifies *"bounded waits"*; neither the attach nor the detach path has one.

**Note:** this is a design tension, not a simple bug — a bounded join inside `DllMain` reintroduces loader-lock risk. The correct resolution is a short bounded wait (e.g. 50–100 ms) on a completion event, accepting a leaked detour on timeout rather than risking execution in unmapped memory.

### R6 — MEDIUM: Doc/reality divergence is now a systemic risk, not a documentation chore

Three independent contradictions were found in a single review pass:

| Claim | Source | Reality |
|:---|:---|:---|
| "23/23 CTest passing", suite rated 10.0/10 | `scorecard_audit.md` | 70 registered; 21 sources never run |
| All 10 pipeline stages ✅ | `BASELINE.md` §3 | No head tracking; no test exercises the assembled path |
| "Host process crash prevented!" | `seh_shield.h:77` runtime log | Handler always returns `CONTINUE_SEARCH` |

`CLAUDE.md` directs every future agent to treat these documents as authoritative pre-flight context. When authoritative context is wrong, the error is inherited by every subsequent decision. This is the mechanism by which a healthy codebase accumulates confidently-wrong changes.

---

## 📐 Subsystem Coupling Assessment

**Verdict: coupling is well-designed at the edges and badly concentrated at the centre.** The system is not uniformly tangled — it has one hub whose removal would resolve most of the structural debt.

### Layer-by-layer

```
  Injection      →  RuntimeState  →  HookManager  →  {DX11, DX12, Vulkan, Input} hooks
   [CLEAN]           [CLEAN]          [CLEAN]              [CLEAN]
                                                              │
                                                              ▼
                                                     Lifecycle Managers
                                                          [CLEAN]
                                                              │
                                                              ▼
                                                    ┌──────────────────┐
                                                    │ FrameCoordinator │ ◀── ALL COUPLING
                                                    │   (God Object)   │     CONCENTRATES
                                                    └──────────────────┘     HERE
                                                       │      │      │
                                          IGraphicsBackend  OpenXR×4  Discovery×5
                                              [CLEAN]     [CONCRETE]   [SINGLETON]
```

| Boundary | Coupling | Assessment |
|:---|:---:|:---|
| Injector → DLL | **Loose** | Process boundary; `CreateRemoteThread` + `LoadLibrary`. Correct. |
| `DllMain` → `RuntimeState` | **Loose** | Shim only; explicit phase machine behind it. Exemplary. |
| `RuntimeState` → `HookManager` | **Loose** | Two call sites (`:86`, `:101`), single owner. Exemplary. |
| `HookManager` → individual hooks | **Loose** | Transactional install/rollback; `IsLayerActive()` gate. Exemplary. |
| Hooks → Lifecycle Managers | **Moderate** | One-directional; per-API isolation holds. Acceptable. |
| **Lifecycle Managers → `FrameCoordinator`** | **Tight** | All three APIs funnel into one singleton. **This is the choke point.** |
| **`FrameCoordinator` → backends** | **Tight** | Owns `IGraphicsBackend` *and* branches on concrete type 14× |
| **`FrameCoordinator` → OpenXR** | **Tight** | Four concrete manager types by direct `#include`, no interface |
| `FrameCoordinator` → discovery | **Tight** | Camera/depth collectors and lock managers reached as global singletons |
| Renderer → AI | **Moderate** | `StereoPipeline` constructs `NeuralInpainter`/`ComfortGuard` directly, no `IAIBackend` seam |
| Launcher ↔ Engine | **Tight, and manual** | TypeScript settings schema and C++ `VRConfig` synchronized by hand; BUG-06 is the recurring symptom |

### Quantified hub analysis

Graphify `god-nodes` (highest edge count):

| Rank | Node | Edges | Verdict |
|:---:|:---|:---:|:---|
| 1 | `DeviceDispatchTable` | 141 | **Acceptable** — Vulkan's own API surface is genuinely this wide |
| 2 | `ImGuiIO` | 128 | **Acceptable** — third-party vendored |
| 3 | `VulkanDispatchTable` | 102 | **Acceptable** — same rationale as #1 |
| 4 | `ImDrawList` | 101 | **Acceptable** — third-party |
| 8 | `DX12Renderer` | 75 | **Watch** — 696 LOC, second-largest first-party file |
| 9 | `VulkanGraphicsBackend` | 64 | **Watch** |

`FrameCoordinator` does **not** appear in the top-ten by raw edge count — its coupling is qualitative rather than numeric. It has few edges but every one of them crosses an architectural layer, and it sits on the single path that all three rendering APIs must traverse every frame.

### Is the coupling too tight?

**Yes — but narrowly, and the fix is well-scoped.** Seven of eleven measured boundaries are loose or moderate and require no work. The injection, hooking, and lifecycle layers are genuinely well-factored, and the Vulkan layer in particular is better than most production graphics middleware.

Four boundaries are tight, and **all four are edges of the same node.** `FrameCoordinator` is not one problem among many; it is the problem, and decomposing it resolves R3, most of R4's blast radius, and the `IAIBackend` gap simultaneously.

### Recommended decomposition sequence

Ordered by risk-adjusted value, and constrained by the Golden Pipeline contract (`docs/GOLDEN_PIPELINE.md` — inputs, outputs, threading, and GPU execution invariants must be preserved exactly):

1. **Push the 14 backend branches down into `IGraphicsBackend`.** The interface already exists and is already owned by `FrameCoordinator`. Each branch becomes a virtual call. Behavior-preserving, mechanically verifiable, removes the entire API-coupling class.
2. **Extract `FrameProfiler`** (CPU timing, GPU queries, dashboard). Zero interaction with the VR path; safest possible first extraction.
3. **Extract `DiscoveryCoordinator`** (camera + depth collectors and lock managers) behind one interface. Removes four singleton reaches from the hot path.
4. **Introduce `IVRSubmitter`** over the four concrete OpenXR managers, so the renderer stops depending on `XrSwapchainImage*` types directly.
5. **Introduce `SubsystemContext`** — already scoped in `docs/REFACTORING_PLAN.md` — and migrate singletons into it incrementally, which is the only durable fix for R4's destruction-order hazard.

Steps 1 and 2 are behavior-preserving and independently testable today. They should not wait on the R2 head-tracking work, which is feature delivery on a different axis.

---

## Priority Action Matrix

| ID | Action | Severity | Effort | Blocks release? |
|:---|:---|:---:|:---:|:---:|
| R2 | Implement `xrLocateViews` head-pose acquisition and plumb into the frame snapshot | 🔴 Critical | High | **Yes** |
| R1 | Fix the VEH handler's false "crash prevented" log (or implement real continuation) | 🔴 Critical | Trivial | **Yes** |
| R5 | Add a bounded wait on teardown completion in `OnDllProcessDetach` | 🔴 High | Low | **Yes** |
| T2 | Remove the stale `$known` failure allowlist from `release.yml` | 🟡 High | Trivial | No |
| T3 | Reconcile `scorecard_audit.md` against `BASELINE.md`; add a "Head Pose" row to the regression matrix | 🟡 High | Low | No |
| R3.1 | Push the 14 backend branches into `IGraphicsBackend` | 🔴 High | Medium | No |
| T4 | Delete or merge the stale `src/ai_matrix_classifier/` duplicate | 🟡 Medium | Low | No |
| T8 | Classify-and-log before swallowing in the five `catch (...)` sites | 🟡 Medium | Low | No |
| T1 | Migrate the 4 API-drift quarantined tests; write one end-to-end pipeline test | 🟡 Medium | Medium | No |
| T6 | Untrack `depth_inpainter.onnx.data` (66 MB), `dummy.cpp`, `compile_commands.json` | 🟡 Medium | Low | No |
| T2b | Re-enable clang-tidy in CI | 🟡 Medium | Low | No |
| T7 | Move the `HogwartsLegacy` heuristic into a versioned JSON profile | 🟡 Medium | Medium | No |
| T5 | Delete `InputManager` or wire it; mark unbuilt AI models as roadmap in `project_memory.md` | 🟡 Low | Low | No |
| R4 | Introduce `SubsystemContext`; migrate the 36 singletons incrementally | 🔴 High | High | No |

---

## Closing Assessment

NexVR Engine is a **structurally serious project with a credibility problem, not a competence problem.**

The low-level engineering is genuinely strong. The Vulkan layer's documented lock ordering, the fail-closed handle semantics, the transactional hook installer, the SEH-shielded memory reads, and one raw `new` across 85k lines are the marks of an engineer who understands the failure modes of this domain. That work should be defended.

What is missing is the discipline of *measurement matching reality*. A scorecard rating a 23-test suite 10/10 while 70 tests exist. A regression matrix marking OpenXR ✅ while head tracking is unimplemented. A safety handler logging "crash prevented" while returning `CONTINUE_SEARCH`. Each is individually small; together they mean the project's own instruments cannot currently be trusted to say whether it is ready — and for an injection layer that runs inside other people's processes, trustworthy instruments are not optional.

**The two highest-value moves are asymmetric in effort and identical in importance:** implement head-pose acquisition (R2, the product's missing core), and correct every place the documentation or logging overstates what the code does (R1, T3). The first makes it a VR product. The second makes its own reports usable as evidence again.

---

*Read-only review. No source files were modified. Findings verified against HEAD `12017b7` on `fix/vulkan-layer-detour-exclusion`.*
