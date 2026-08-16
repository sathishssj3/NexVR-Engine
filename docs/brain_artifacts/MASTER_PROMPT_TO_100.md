# NexVR Engine — Master Execution Prompt: 68 → 100

**Purpose:** A self-contained brief. Paste the whole file into a fresh Claude Code session, or hand it to a subagent, and it has everything needed to execute without re-deriving context.
**Baseline:** 68/100, measured 2026-08-16 against HEAD `12017b7` + Phase 1 working tree.
**Companion:** [`project_architecture_review.md`](./project_architecture_review.md) — the evidence behind every score below.

---

## 0. Role & Mission

You are a **Principal C++ Software Architect** working on NexVR Engine — a universal VR injection layer (DirectX 11/12, Vulkan → OpenXR) written in C++20 for Windows x64 MSVC.

Your mission is to raise the engine from **68/100** to the highest honest score achievable, using the rubric in §2 and the phase plan in §4.

**Operating mode:** `max` / ultracode for Phase 4 and Phase 5; `high` for Phase 3; `medium` for Phase 2.

### The one rule that outranks the score

> **Never move a number by changing the rubric.**

This codebase previously carried a scorecard rating a 23-test suite "10.0/10" while 70 tests existed, and a VEH handler logging *"Host process crash prevented!"* while returning `EXCEPTION_CONTINUE_SEARCH`. Both were closed in Phase 1. A score is only worth having while it is hard to get. If a task is blocked, report it blocked — do not re-weight, re-scope, or soften an acceptance criterion to make a phase look complete.

---

## 1. Non-Negotiable Invariants

Every change must preserve these. Violating one is a failed task regardless of what else it achieved.

### 1.1 The Golden Pipeline contract

`docs/GOLDEN_PIPELINE.md` freezes eight stages — Injection → Hook Installation → Graphics Detection → Swapchain Interception → Camera & Depth Discovery → Stereoscopic Projection → OpenXR Composition → Frame Submission & Pacing. **Inputs, outputs, threading model, and GPU execution order of each stage are contractual.** Refactors may move code between files freely; they may not change what crosses a stage boundary or on which thread.

### 1.2 Closed defects that must stay closed

From `docs/project_memory.md`:

| ID | Invariant |
|:---|:---|
| BUG-01/02 | `HookManager` exclusively owns MinHook lifecycle (`MH_Initialize`/`MH_Uninitialize`). Never re-add these to individual hook files. |
| BUG-05 | `dllmain.cpp` / `RuntimeState` close worker thread handles. |
| BUG-06 | `config_manager.cpp` serialization stays in sync with the launcher TypeScript schema. Edit both sides together. |
| BUG-09 | `injectionManager.ts` uses bounded polling. |
| DEAD-02/04 | Shared DX11 hook globals stay thread-safe (`std::atomic`). |
| DEAD-05 | `DllMain` is a minimal shim. All work deferred to `RuntimeState`. |
| QUAL-01 | Single unified injector at `src/injector/main.cpp`. No duplicates under `tools/`. |
| QUAL-04 | No game-specific hardcoded logic in `libraryManager.ts`. |

### 1.3 Architecture rules

1. **Dual-mode rendering.** When `shouldAttemptStereo == false` (menus, loading, pre-lock), **do not skip OpenXR submission** — copy the backbuffer to both eye swapchain images.
2. **DX12 single-queue contract.** All stereo compute dispatches and copies go to `Dx12LifecycleManager::Get().GetMainQueue()` for strict FIFO with game rendering.
3. **Vulkan dispatch safety.** Never return hooked pointers from `vkGetDeviceProcAddr`/`vkGetInstanceProcAddr` fallbacks — always the trampoline. Untracked devices stay pure pass-through or fail closed; never return `VK_SUCCESS` with an uninitialized handle.
4. **Layer/detour exclusion.** `IsLayerActive()` (`src/hooks/vulkan_layer.cpp:32`, consumed at `src/core/hook_manager.cpp:85`) suppresses all MinHook detours when the Vulkan layer is live. Running both corrupts the heap.

### 1.4 Code that is exemplary — do not "clean up"

- `src/hooks/vulkan_layer.cpp` lines 151–176: next-layer pointers resolved *before* the lock (because `gdpa()` re-enters the layer chain), and the comment documenting that `g_dispatchMutex` and `g_swapchainMutex` are never nested. Both comments are load-bearing.
- `src/core/seh_shield.h` `SafeReadMemory`: zeroes the destination on AV so partial reads never become matrix candidates.
- `src/core/hook_manager.h`: the `Prepare → Validate → Install → Verify → Commit` phase machine with `m_rollbackStack`.

### 1.5 Process rules

- **Pre-flight with graphify, not grep.** `graphify query "<concept>"`, `graphify explain "<class>"`, `graphify path "<A>" "<B>"` before opening shared headers. Never blind global search-and-replace.
- **Run `graphify update .` after every `.cpp`/`.h` change.** AST-only, instant, zero token cost.
- **No destructive git.** `git reset --hard`, `git clean -fd`, `git push --force` require explicit human approval.
- **Atomic reversible diffs.** Verify each against `git status` / `git diff` before moving on.
- **Anti-cheat fence.** Tier 1 (no AC) supported; Tier 2 (AC disabled by user) supported offline; Tier 3 (kernel AC — Vanguard, BattlEye, active EAC, Ricochet) **never** targeted, never bypassed.

---

## 2. Scoring Rubric

Self-score after every phase. Show the arithmetic; a bare number is not a report.

| # | Dimension | Weight | Baseline | What 10.0 requires |
|:--|:---|:---:|:---:|:---|
| 1 | Build & Toolchain | 10% | 9.0 | Zero compiler/shader/CMake-policy warnings in Debug and Release |
| 2 | Defensive Programming & Concurrency | 20% | 7.5 | No unsatisfiable waits, deterministic teardown order, every shared mutation guarded, no false safety signals |
| 3 | Architecture & Coupling | 20% | 5.5 | No God Object, no runtime API-type branching behind a polymorphic handle, singletons behind an explicit context |
| 4 | Test Posture | 15% | 6.0 | Every test source compiles and runs; assembled VR pipeline exercised in CI |
| 5 | CI/CD Integration | 10% | 8.5 | All analyzers enforcing, coverage gate, sanitizer job, zero known-failure allowlists |
| 6 | Documentation & Knowledge Systems | 10% | 9.5 | Every claim in every doc verifiable against the tree on the day it is read |
| 7 | Repository Hygiene | 5% | 3.5 | No build artifacts or model weights tracked; `.gitignore` valid and effective |
| 8 | Functional Delivery (end-to-end VR) | 10% | 4.0 | Verified game catalog meeting the scorecard's own gates |
| | **Total** | **100%** | **68** | |

**Scoring honesty check.** Before writing a score, answer: *what command did I run that would have produced a lower number if I were wrong?* If there is no such command, the score is a guess — label it one.

---

## 3. Verification Protocol

Run after **every** task, not every phase.

```bash
cmake --build build --config Release
```

```bash
ctest --test-dir build -C Release --output-on-failure
```

```bash
graphify update .
```

```bash
git status --porcelain && git diff --stat
```

**Build-freshness gate.** Before trusting any test result, confirm the binaries postdate the sources:

```bash
ls -lt --time-style=+%H:%M build/bin/vrinject.dll $(git diff --name-only | grep -E '\.(cpp|h)$')
```

A green suite against a stale build proves nothing. This check is cheap and the failure it prevents is silent.

---

## 4. The Phase Plan

Phases 2 and 3 are strictly ordered. Phases 4 and 5 are independent of each other and of Phase 3 — see §5 for sequencing.

---

### PHASE 2 — Finish Phase 1 · 68 → 72 · ~1 day · effort `medium`

Three defects from the Phase 1 pass. Highest points-per-hour in the plan.

#### 2.1 — R5: make the teardown wait satisfiable (+1.0)

**Current state** (`src/core/runtime_state.cpp:34`):

```cpp
void RuntimeState::OnDllProcessDetach() {
    TransitionTo(RuntimePhase::Stopping);
    std::thread teardownThread(&RuntimeState::BackgroundTeardown, this);
    std::unique_lock<std::mutex> lock(m_stateMutex);
    m_stateCv.wait_for(lock, std::chrono::milliseconds(100), [this]() {
        return m_phase.load() == RuntimePhase::Stopped;
    });
    teardownThread.detach();
}
```

**Why this cannot work.** `OnDllProcessDetach` runs inside `DllMain`, holding the loader lock. A newly created thread cannot reach its entry point until `LdrpInitializeThread` acquires that same lock — released only when `DllMain` returns. (`DisableThreadLibraryCalls` suppresses the DLL_THREAD_ATTACH *callback*; the loader still takes the lock during thread init.) The predicate can never become true. Net effect: a guaranteed 100 ms stall on every process exit, with the teardown/unload race entirely unfixed and the code reading as though it were solved.

**Required shape.** The signalling thread must already exist and be past loader initialization.

1. At the end of `BackgroundInitialize`, transition to `Running`, then **park the existing worker** on `m_stateCv` waiting for `m_phase == Stopping`.
2. On wake, the worker runs the current `BackgroundTeardown` body, transitions to `Stopped`, and notifies.
3. `OnDllProcessDetach` becomes: `TransitionTo(Stopping)` → `notify_all()` → bounded `wait_for(100ms)` for `Stopped`. **No thread construction anywhere in `DllMain`.**

**Acceptance:**
- `grep -n "std::thread" src/core/runtime_state.cpp` shows exactly one construction, in `OnDllProcessAttach`.
- Teardown-complete log line appears before process exit in a real injection.
- No measurable stall on game quit.

#### 2.2 — T6: repository hygiene, actually applied (+2.25)

**Current state.** `depth_inpainter.onnx.data` (66 MB) is still tracked. `.gitignore` was appended as UTF-16LE with no leading newline, producing:

```
o p e n v r _ c a p i . h d \0 e \0 p \0 t \0 h \0 ...
```

Two failures: the new text fused onto the tail of the `openvr_capi.h` rule, and every appended line carries interleaved NUL bytes. `git check-ignore -v dummy.cpp compile_commands.json` returns **nothing** — none of the new rules are active, and the pre-existing `openvr_capi.h` rule is now broken. `file .gitignore` reports `data`.

Cause: Windows PowerShell 5.1 `>>` / `Out-File` default encoding.

```bash
git checkout .gitignore && printf '\ndepth_inpainter.onnx.data\ndummy.cpp\ncompile_commands.json\n' >> .gitignore && git rm --cached depth_inpainter.onnx.data
```

**Acceptance:**
- `file .gitignore` reports ASCII text (not `data`).
- `git check-ignore -v depth_inpainter.onnx.data dummy.cpp compile_commands.json openvr_capi.h` prints a rule for all four.
- `git ls-files | xargs -I{} du -k {} 2>/dev/null | sort -rn | head -5` shows no file over ~1 MB.

#### 2.3 — T2b: make clang-tidy survivable (+0.5)

`.github/workflows/release.yml` sets `header-filter: '.*'` with `clang-tidy-warnings-as-errors: '*'`. That lints every vendored header — `vulkan_core.h` (26,929 lines), `openxr.h` (13,237), `openxr_reflection.h` (10,886), `imgui.h` (4,256) — as errors. First push will almost certainly go red on third-party code.

Narrow to first-party: `header-filter: '^src/(core|hooks|rendering|openxr|ai|injector)/'`. Keep warnings-as-errors. Add a `.clang-tidy` at repo root with an explicit check list rather than inheriting defaults.

**Acceptance:** one full CI run green on both matrix configurations, with clang-tidy reporting a non-zero number of files analyzed (a filter so narrow it analyzes nothing is not a pass).

---

### PHASE 3 — Head Tracking · 72 → 76 · ~1 week · effort `high`

R2. The product's missing core. Functional Delivery 4.0 → 8.0.

**Current state:** `xrLocateViews` and `xrLocateSpace` appear **only** in the vendored SDK header `src/rendering/openxr/openxr.h` (typedef :1729, prototype :1924). Zero call sites in NexVR code. Whatever currently reaches `xrEndFrame` is a fixed-pose image in a headset.

**Plan before code.** This crosses `RenderFrameSnapshot`, all three graphics backends, and Golden Pipeline stages 6–8. Write the design first; get it reviewed; then implement.

#### 3.1 — Acquire

In `src/openxr/openxr_frame_submitter.cpp`:

- Create the reference space once: `XR_REFERENCE_SPACE_TYPE_STAGE`, falling back to `LOCAL` if unsupported.
- Per frame, call `xrLocateViews` with `XrViewLocateInfo{ viewConfigurationType, displayTime, space }` where **`displayTime` is `XrFrameState::predictedDisplayTime` from `xrWaitFrame`** — not a current timestamp. Using "now" produces a pose that is one frame stale, which reads as tracking lag and is a common source of discomfort.
- Check `XrViewState::viewStateFlags` for `XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT`. On invalid, hold the last good pose — do not submit garbage and do not skip submission (invariant §1.3.1).

#### 3.2 — Plumb

Add per-eye `XrPosef` and `XrFovf` to `RenderFrameSnapshot`. Every producer must populate them; no defaulted zero-pose fallbacks that silently look like "facing forward."

#### 3.3 — Compose

Combine the head pose with the game's discovered view matrix.

> **The handedness probe from commit `70c73c1` is the input here.** Get the Z convention wrong and the world rotates opposite the head. That is not a subtle bug — it is instantly nauseating and will be reported as "tracking is broken." Log the composed basis on first lock and verify against a known-good reference before shipping.

#### 3.4 — Project

Replace the hardcoded projection in the stereo compute shader with the real per-eye `XrFovf`. Asymmetric FOV is normal in VR headsets; a symmetric assumption produces subtle mismatch between eyes.

**Acceptance:**
- Rotating the headset moves the rendered view in the **same** direction, on all three backends.
- IPD from config produces measurable stereo separation.
- `docs/BASELINE.md` "Head Pose Acquisition" row flips ❌ → ✅ **only after** in-headset verification, never on code-complete.

---

### PHASE 4 — Decompose FrameCoordinator · +9.0 · ~2 weeks · effort `max`

Architecture 5.5 → 9.0, Concurrency 8.0 → 9.0. Largest single block in the plan.

**Current state.** `src/core/frame_coordinator.cpp` holds `std::unique_ptr<IGraphicsBackend>` — the polymorphic seam exists — and still contains **14 explicit backend comparisons** on the frame path at lines `43, 57, 60, 125, 158, 161, 164, 175, 202, 219, 274, 300, 343, 399`. Line 399 checks the polymorphic handle exists and then re-checks its concrete type. The class simultaneously owns engine detection, CPU profiling, GPU profiling, camera discovery, depth discovery, compatibility scoring, four OpenXR managers, and dashboard rendering. All three hooks plus the Vulkan layer include its header.

Execute in order. **Each step ships independently and must leave the suite green.**

| Step | Change | Gain |
|:---:|:---|:---|
| 4.1 | Push all 14 backend branches into `IGraphicsBackend` as virtual calls. Interface already exists and is already owned — this is behavior-preserving and mechanically verifiable. | Removes the entire API-coupling class |
| 4.2 | Extract `FrameProfiler` (CPU timing, GPU queries, dashboard). Zero VR-path interaction — safest extraction, do it early to build confidence. | −3 responsibilities |
| 4.3 | Extract `DiscoveryCoordinator` (camera + depth collectors, lock managers) behind one interface. | −4 singleton reaches from the hot path |
| 4.4 | Introduce `IVRSubmitter` over the four concrete OpenXR managers, so the renderer stops depending on `XrSwapchainImage*` types. | Breaks renderer↔OpenXR concretion |
| 4.5 | Introduce `SubsystemContext` (already scoped in `docs/REFACTORING_PLAN.md`) and migrate the 36 Meyer's singletons incrementally. | **Closes R4** |

**Why 4.5 is the keystone.** Meyer's singletons destruct in reverse order of first construction — an order set by runtime call sequence, which varies per game, per injection timing, and per API. During `DLL_PROCESS_DETACH` that ordering is effectively nondeterministic: `VulkanDispatchTable` can be destroyed while `VulkanLifecycleManager` still holds handles routed through it. Explicit ownership makes destruction order a compile-time fact. Steps 4.1–4.4 remove coupling; 4.5 removes the *ability* to reintroduce it, because a subsystem absent from the context cannot be reached via `::Get()` from anywhere.

**Acceptance per step:** `grep -cE "GraphicsBackend::(DX11|DX12|Vulkan)" src/core/frame_coordinator.cpp` strictly decreasing, reaching 0 after 4.1. Full suite green after each. `graphify explain "FrameCoordinator"` shows monotonically decreasing degree.

---

### PHASE 5 — Make the Pipeline Testable · +8.75 · ~2 weeks · effort `max`

Test Posture 6.0 → 9.5, plus dimensions 1, 5, 6, 7 to 10.0.

**Current state.** 95 test sources; 70 registered; **21 never run** — 7 quarantined at `CMakeLists.txt:770`, 12 zero-byte, 2 excluded for duplicate `main`. Every registered test is a unit test. Nothing exercises the assembled VR frame path. The Golden Pipeline contract has no executable enforcement. Launcher: 2 specs against 18 TypeScript sources.

#### 5.1 — Clear the quarantine (4 API-drift entries)

| Test | Drift |
|:---|:---|
| `test_ai_pipeline` | `AICommandQueue` type deleted |
| `test_camera_tracker` | `CameraDeltaTracker::UpdateDelta` → `UpdateCandidateMotion`, new signature |
| `test_depth_tracker` | `GraphicsResourceIdentity::creationGeneration` field removed |
| `test_dx11_lifecycle` | `Dx11ContextSnapshot` type removed |

Migrate to the current API. Delete `AUTO_GTEST_QUARANTINE` entries as each compiles.

#### 5.2 — Unblock the 3 COM-mock suites

`test_dx12_descriptor_tracker`, `test_dx12_fence_manager`, `test_dx12_resource_state_tracker` fail a gmock 1.14 `static_assert` on `__stdcall` COM interfaces. Either upgrade gmock past 1.14 or hand-roll COM fakes. Toolchain problem, not a test-quality problem — do not rewrite the tests to dodge it.

#### 5.3 — Resolve the 12 zero-byte files

`e2e_test_app.cpp`, `test_beta_validation.cpp`, `test_depth_pipeline.cpp`, `test_game_compatibility.cpp`, `test_performance_pipeline.cpp`, `test_reconstruction_quality.cpp`, `test_release_pipeline.cpp`, `test_runtime_capture.cpp`, `test_runtime_integration.cpp`, `test_stereo_pipeline.cpp`, `test_temporal_pipeline.cpp`, `test_vr_runtime.cpp`.

**Fill or delete. A named empty test is worse than no test** — it reads as coverage in every directory listing and in every future audit.

#### 5.4 — Build the headless VR harness ← the keystone

A mock OpenXR runtime plus a synthetic present loop per backend, so the assembled pipeline runs in CI with no headset attached.

Minimum viable surface: `xrCreateInstance`, `xrGetSystem`, `xrCreateSession`, `xrCreateReferenceSpace`, `xrEnumerateSwapchainFormats`, `xrCreateSwapchain`, `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`, `xrAcquire/Wait/ReleaseSwapchainImage`, `xrEndFrame`. Feed scripted head poses so composition math is assertable, not eyeballed.

This is what converts the Golden Pipeline from a document into an assertion — and it is what makes Phase 4 safe.

#### 5.5 — Close the mechanical dimensions

- **Dim 1 → 10.0:** fix `stereo_reprojection.hlsl(64,78)` X3203 signed/unsigned, `tonemap.hlsl(9)` X3577 isnan, and CMake policies `CMP0169` / `CMP0148`.
- **Dim 5 → 10.0:** coverage gate; an ASan/UBSan job; confirm zero known-failure allowlists remain.
- **Dim 6 → 10.0:** re-verify every doc claim against the tree; keep the "21 excluded" style of caveat that Phase 1 introduced.
- **Dim 7 → 10.0:** verify `.git` size dropped after the 66 MB untrack; audit remaining tracked artifacts.

**Acceptance:** `AUTO_GTEST_QUARANTINE` is empty. Zero zero-byte test sources. At least one CI test drives Golden Pipeline stages 4→8 end to end. Zero build warnings in both configurations.

---

## 5. Sequencing

Phases 2 → 3 are ordered. Phase 4 and Phase 5 are independent of Phase 3 and of each other.

**Recommended: Phase 5 before Phase 4.** Refactoring a God Object with only unit tests underneath is how a working pipeline breaks silently. The headless harness turns Phase 4 from risky into mechanical. If parallelizing across two people, run Phase 3 (OpenXR submitter, snapshot) alongside Phase 4 (FrameCoordinator internals) — the file sets barely overlap.

| Milestone | Score | Cumulative effort |
|:---|:---:|:---:|
| Baseline | 68 | — |
| After Phase 2 | 72 | 1 day |
| After Phase 3 | 76 | +1 week |
| After Phase 5 | 85 | +2 weeks |
| After Phase 4 | 94 | +2 weeks |

**If the goal is "ship a working VR injector" rather than a score, stop at 76.** Phases 2 and 3 deliver the product. Phases 4 and 5 are quality debt that can be paid down while shipping.

---

## 6. The Last 6 Points

**~94 is the engineering ceiling. 100 requires field data no commit produces.**

**Functional Delivery 8.0 → 10.0** is gated by the project's own scorecard: 10 verified titles across Unreal/Unity/RE Engine, 99.5% crash-free sessions, 97% supported-session success. That is a compatibility program measured in months of testing across engines, GPU drivers, and headsets.

**Architecture 9.0, Concurrency 9.0, Test Posture 9.5 → 10.0** are diminishing returns: eliminating all 226 `reinterpret_cast` sites, formally proving lock-freedom on the render path. Real work, low payoff relative to anything in Phases 2–5.

Reaching **94 with head tracking shipped and the harness built** means a genuinely good engine and a scoreboard that can be trusted. That is the actual goal. The remaining 6 points are earned by users, not by the compiler.

---

## 7. Reporting Format

At the end of every phase, produce exactly this:

```markdown
## Phase N Complete — <score before> → <score after>

### Verified Done
| Task | Acceptance command run | Result |
|---|---|---|
| N.1 | `<exact command>` | `<exact output>` |

### Blocked / Deferred
| Task | Why | What would unblock it |
|---|---|---|

### Score Delta
| Dim | W | Before | After | Evidence |
|---|---|---|---|---|
...
Weighted total: <arithmetic shown>

### Invariants Re-verified
- [ ] Golden Pipeline stage boundaries unchanged
- [ ] MinHook ownership still exclusive to HookManager
- [ ] Vulkan layer/detour exclusion intact
- [ ] DX12 single-queue contract intact
- [ ] Full suite green against a fresh build (timestamps checked)
```

**Anything not verified by a command you actually ran is reported as unverified.** Not "done", not "should work" — unverified. That distinction is the whole reason this document exists.

---

*Baseline established 2026-08-16 against HEAD `12017b7` plus the Phase 1 working tree. Every line number and count in this document was verified against the tree on that date; re-verify before relying on any of them.*
