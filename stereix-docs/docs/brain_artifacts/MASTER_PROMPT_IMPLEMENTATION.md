# NexVR Engine — Master Execution Prompt: Lever Implementation Runbook

**Purpose:** Execution mechanics for the six levers. Paste into a fresh Claude Code session and it can execute without re-deriving context.
**Companions:** [`MASTER_PROMPT_MAINTAINABILITY.md`](./MASTER_PROMPT_MAINTAINABILITY.md) states *what* each lever is and *why*. This states *how*, *in what order*, and *what to do when it goes wrong*.
**Baseline measured:** 2026-08-16 23:25 IST.

---

## 0. Read This First — Three Corrections to the Proposed Plan

### 0.1 The order is swapped: harness first

The maintainability brief numbered `SubsystemContext` as Lever 1 and the harness as Lever 2. **Execute them in the opposite order.**

Migrating 39 subsystems across 110+ detour call sites with nothing exercising the assembled pipeline is the exact risk the harness removes. The Vulkan layer path is the worst case — no test covers it, and it activates differently from the detour path. Build the net, then walk the wire.

Throughout this document, **L2 (harness) precedes L1 (context)**. Lever numbering is kept for cross-reference.

### 0.2 The singleton floor is 1, not 0

Measured `::Get()` calls inside `src/hooks/`:

| Subsystem | Calls from detours |
|:---|:---:|
| `VulkanDispatchTable` | 47 |
| `VulkanQueueManager` | 17 |
| `VulkanLifecycleManager` | 15 |
| `VulkanResourceTracker` | 7 |
| `FrameCoordinator` | 6 |
| others | ~20 |

These live in free functions whose signatures the *game* controls — `hkQueuePresentKHR(VkQueue, const VkPresentInfoKHR*)`. There is no `this` and no place to put a context parameter. **Constructor injection cannot reach them.**

Use an **ambient root** (§3.1): exactly one surviving `::Get()`, returning `SubsystemContext&`. Ratchet 4.1's target is **1**, and reaching 1 is complete success. A plan aiming at 0 either fails or smuggles in something worse — a global pointer, a thread-local, a re-entrant initialiser on the render thread.

### 0.3 Ratchets are pre-work, not part of Lever 1

`FrameCoordinator` degree went **34 → 42 in ten minutes** during planning. Singletons went **36 → 39** during the earlier reorg. Neither was intentional. `scripts/check_coupling*` and `docs/coupling_budget.txt` do not exist.

**Install the ratchets before touching any lever.** Without them you cannot tell whether L1 is working, and its gains will leak back the same way these did.

---

## 1. Pre-Flight — blocking gates

Do not begin any lever until all four pass.

### 1.1 Commit the tree

420 uncommitted changes across `src/` and `tests/`. The reorg plus Phase 3/4 work is unversioned and one `git checkout` from gone. A large refactor on a dirty tree has no rollback point.

```bash
git status --porcelain -- src/ tests/ | wc -l
```

**Gate:** returns 0.

### 1.2 Install the ratchets

Create `docs/coupling_budget.txt`:

```
singletons=39
backend_enum_leaks=7
hardcoded_game_names=1
test_sources=80
test_registered=74
framecoordinator_degree=42
```

Create `scripts/check_coupling.ps1` measuring each per §4 of the maintainability brief, comparing to the budget, exiting non-zero on any increase. Wire it into `.github/workflows/release.yml` before the test step.

**Gate:** the script passes on a clean tree, and fails when you deliberately add one `::Get()`. Verify both directions — a ratchet that never fails is decoration.

### 1.3 Green suite against a fresh build

```bash
cmake --build build --config Release && ctest --test-dir build -C Release --output-on-failure
```

```bash
ls -lt --time-style=+%H:%M build/bin/vrinject.dll $(git log -1 --name-only --pretty=format: | grep -E '\.(cpp|h)$' | head -5)
```

**Gate:** all registered tests pass, DLL postdates every source.

### 1.4 Sync the knowledge graph

```bash
graphify update .
```

The index is stale after the reorg — it still points `stereo_pipeline.h` at `src/rendering/` when it lives at `src/rendering/stereo/`. Every `graphify path` answer is unreliable until this runs.

**Gate:** `graphify explain "StereoPipeline"` reports the current path.

---

## 2. LEVER 2 (first) — The Headless Harness

### 2.1 Build order

1. **`MockOpenXRRuntime`** in `tests/harness/`. Implement the surface: `xrCreateInstance`, `xrGetSystem`, `xrCreateSession`, `xrCreateReferenceSpace`, `xrEnumerateSwapchainFormats`, `xrCreateSwapchain`, `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`, `xrAcquire/Wait/ReleaseSwapchainImage`, `xrEndFrame`.
2. **Scripted pose source.** `xrWaitFrame` returns a deterministic `predictedDisplayTime`; `xrLocateViews` returns poses from a scripted track. Start with three: identity, 90° yaw left, 0.5 m forward translation.
3. **Synthetic present loop per backend.** Minimal device + swapchain, drive `FrameCoordinator::OnPresentBegin` with a populated `RenderFrameSnapshot`.
4. **Assertions on composed output**, not on "it ran". The 90°-left case must produce a view matrix whose forward vector rotated *right*. That single assertion is what catches an inverted `isLeftHanded` forever.

### 2.2 Mutation check — mandatory

A test that cannot fail is not a test. For each of the three poses, deliberately invert one sign in the composition path, confirm red, revert.

**Record the result in the phase report.** An unmutated harness is unverified regardless of how green it looks.

### 2.3 Acceptance

- One CI test drives Golden Pipeline stages 4→8 on all three backends.
- Mutation check documented for all three pose cases.
- `test_registered` ratchet increases.

---

## 3. LEVER 1 (second) — SubsystemContext

### 3.1 The ambient root

One surviving singleton. Everything else hangs off it.

```cpp
// src/core/subsystem_context.h
class SubsystemContext {
public:
    // The ONLY surviving ::Get() in the codebase.
    // Exists because hook detours have game-controlled signatures and
    // cannot accept a context parameter. See MASTER_PROMPT_IMPLEMENTATION.md §0.2.
    static SubsystemContext& Get();

    // Accessors return references; the context owns lifetime.
    vulkan::VulkanDispatchTable& VulkanDispatch();
    // ...one per subsystem, added as each is migrated.

private:
    // Declaration order IS destruction order (reverse). This is the point.
    // Leaves first, dependents after.
    std::unique_ptr<Logger>                      m_logger;
    std::unique_ptr<DiagnosticContext>           m_diagnostics;
    std::unique_ptr<vulkan::VulkanDispatchTable> m_vulkanDispatch;
    // ...
};
```

**The comment on `Get()` is load-bearing.** Without it, a future reader deletes the "last singleton" and breaks every detour.

Construct in `RuntimeState::BackgroundInitialize`, destroy in `BackgroundTeardown` — both already exist and already run off the loader lock.

### 3.2 Migration order — leaves first, layer last

Migrating in the wrong order means a half-migrated subsystem reaching a not-yet-migrated one through a stale `::Get()`.

| Tier | Subsystems | Why this tier | Risk |
|:---:|:---|:---|:---:|
| **T1** | `Logger`, `DiagnosticContext`, `CapabilityRegistry`, `SprintCompatibilityLogger` | Zero inbound deps. Pure leaves. | low |
| **T2** | `MatrixClassifier`, `TemporalCameraFilter`, `TemporalDepthFilter`, `CandidateCollector`, `DepthCandidateCollector`, `CameraLockManager`, `DepthLockManager`, `MemoryScanner` | Depend only on T1. Covered by existing tests. | low |
| **T3** | `EngineDetector`, `UnityScanner`, `UnrealScanner`, `UniversalScanner` | Depend on T1–T2. | med |
| **T4** | `Dx11LifecycleManager`, `Dx12LifecycleManager`, `Dx12DescriptorTracker` | Detour-reached. Harness now covers them. | med |
| **T5** | `FrameCoordinator` | 42 edges. Do after T1–T4 so most of its reaches are already context-routed. | high |
| **T6** | `VulkanDispatchTable`, `VulkanQueueManager`, `VulkanLifecycleManager`, `VulkanResourceTracker`, `VulkanDescriptorTracker`, `VulkanCameraExtractor`, `VulkanImage*Tracker`, `VulkanFramebufferTracker`, `VulkanRenderPassTracker`, `VulkanBufferResolver`, `VulkanDepth*` | **Last.** 47+17+15 detour calls, two activation modes (layer and detour), and the layer path is the least test-covered code in the repo. | **high** |

**T6 last is not negotiable.** The Vulkan layer already produced one heap-corruption crash this month (`0xC0000409`, dispatch-map races) and a still-unresolved `0xd4805`. Migrating it before T1–T5 are proven adds an unbounded variable to an already-open investigation.

### 3.3 Per-subsystem recipe

For each subsystem, one commit:

1. `graphify explain "<Subsystem>"` — record inbound edge count.
2. Add a `std::unique_ptr` member to `SubsystemContext` in dependency order, plus an accessor.
3. Constructor takes its dependencies as references. No `::Get()` inside.
4. Replace call sites: members and methods take the dependency by reference; **detour free functions use `SubsystemContext::Get().X()`**.
5. Delete the subsystem's `static T& Get()`.
6. Build, full suite, harness.
7. `scripts/check_coupling.ps1` — singleton count must have dropped by exactly 1.
8. Lower `singletons=` in the budget file. **Commit both together**, so budget and tree never disagree.

### 3.4 Rollback

One subsystem per commit, so any single migration reverts with `git revert <sha>` without touching the others. If a tier goes wrong, revert the tier and re-plan — do not fix forward across a broken graph.

### 3.5 Acceptance

- `singletons` reaches **1** (`SubsystemContext::Get()`).
- Destruction order readable in one file, in `SubsystemContext`'s member declaration block.
- `FrameCoordinator` degree strictly decreasing across T5.
- Suite and harness green after every commit, not just at the end.

---

## 4. LEVER 3 — Backend Enum Isolation

7 files outside `src/rendering/` branch on `GraphicsBackend::`: `compatibility_scorer.cpp`, `frame_coordinator.cpp`, `graphics_types.h`, `openxr_runtime_manager.cpp`, `openxr_swapchain_manager.cpp`, plus two in `src/heuristics/`.

For each branch, ask **what capability is this actually testing?** `if (backend == DX12)` in a swapchain manager usually means "does this backend need explicit LUID matching?" — that is a capability query, not an identity check. Add the capability to `IGraphicsBackend`, not another branch.

`graphics_types.h` may *define* the enum. Nothing outside `src/rendering/` may *branch* on it.

**Acceptance:** ratchet 4.2 = 0. Adding a backend touches one directory plus one registration line — verify by writing a stub `NullGraphicsBackend` and counting the diff.

---

## 5. LEVER 4 — Data-Driven Game Profiles

Replace `engine_detector.cpp:83` with versioned JSON in `profiles/`. Required fields: executable match, engine hint, camera/depth heuristic tuning, **anti-cheat tier**, notes.

Tier 3 (Vanguard, BattlEye, active EAC, Ricochet) is rejected **at profile load**, never at runtime — a rejected profile can never reach injection.

**Acceptance stronger than the proposed keyword grep** — a list of game names would not catch `"Starfield"`. Make it structural: zero executable-name string comparisons in `engine_detector.cpp`. Verify by adding a title with no rebuild.

---

## 6. LEVER 5 — Unified Config Schema

One source generating `config_manager.cpp`'s struct and the launcher TypeScript interface. Closes BUG-06 as a class.

**Acceptance:** add a field in one place, both sides regenerate. A deliberate hand-edit desyncing them fails the build.

---

## 7. LEVER 6 — IAIBackend Routing

`IAIBackend` exists at `src/ai/backend/ai_backend.h:30`. Remove the concrete `NeuralInpainter` / `ComfortGuard` members from `src/rendering/stereo/stereo_pipeline.h:69–70` and register stages instead.

**Budget correction.** 11.1 ms is the *total frame* budget. `project_memory.md` §5 puts the **synchronous** AI path under **1.5 ms** total across all sync stages, with the memory transformer and inpainter mask generation on the async worker path. Enforcing 11.1 ms would let AI consume the entire frame and miss every vsync.

Each stage declares budget in milliseconds and path (sync / async). The pipeline skips a sync stage that would exceed the remaining sync budget — degrade, never drop the frame.

**Acceptance:** `grep -c "Inpainter\|Comfort\|Predictor\|Synthesizer" src/rendering/stereo/stereo_pipeline.h` = 0. A stage declaring 2.0 ms sync is refused at registration.

---

## 8. Invariants — never traded for structure

A refactor violating any of these has failed regardless of coupling removed.

1. **Golden Pipeline contract** — eight stages; inputs, outputs, threading, GPU order across boundaries are contractual.
2. **MinHook ownership** — `HookManager` exclusively owns `MH_Initialize`/`MH_Uninitialize` (BUG-01/02).
3. **DX12 single queue** — stereo dispatches and copies to `Dx12LifecycleManager` main queue, strict FIFO.
4. **Vulkan dispatch safety** — never return hooked pointers from `vkGet*ProcAddr` fallbacks; untracked devices pass through or fail closed.
5. **Layer/detour exclusion** — `IsLayerActive()` suppresses detours when the layer is live.
6. **Dual-mode rendering** — `shouldAttemptStereo == false` still submits; copy backbuffer to both eyes.
7. **Head-pose fallback** — invalid `viewStateFlags` holds last good pose, never identity.
8. **DllMain is a shim** — no work, no thread creation, no waiting on threads created there (DEAD-05).
9. **No new `::Get()`** outside `SubsystemContext`.
10. **Anti-cheat fence** — Tier 3 never targeted, never bypassed.

Process: `graphify` before touching shared headers; `graphify update .` after every `.cpp`/`.h` change; no `git reset --hard` / `clean -fd` / `push --force` without explicit human approval.

---

## 9. Reporting Format

After every lever — and after every tier within Lever 1:

```markdown
## <Lever N / L1 Tier T> — <name>

### Ratchets
| Ratchet | Budget | Measured | Direction |
|---|---|---|---|
| singletons | 39 | <n> | ↓ / — / ↑ FAIL |
| backend_enum_leaks | 7 | <n> | |
| hardcoded_game_names | 1 | <n> | |
| test_sources / registered | 80 / 74 | <n>/<n> | |
| framecoordinator_degree | 42 | <n> | |

### Verified
| Task | Command run | Actual output |
|---|---|---|

### Blocked
| Task | Why | What would unblock it |
|---|---|---|

### Invariants
- [ ] Golden Pipeline boundaries unchanged
- [ ] MinHook ownership exclusive
- [ ] Vulkan layer/detour exclusion intact
- [ ] DX12 single-queue contract intact
- [ ] No test deleted, no ratchet budget raised
- [ ] Suite + harness green against a fresh build
- [ ] Budget file and tree committed together

### Budget changes
Lowered: <which, and to what>. Raised: <which, and the explicit decision authorising it>.
```

**Report unverified work as unverified** — not "done", not "should work". A ratchet you did not run is a ratchet that did not pass. This project has twice paid a full audit to discover an overstated completion.

---

*Baseline measured 2026-08-16 23:25 IST: singletons 39, enum leaks 7, hardcoded game names 1, sources 80 / registered 74, `FrameCoordinator` degree 42 (up from 34 at 23:15), 420 uncommitted changes in `src/` + `tests/`, graphify index stale. Re-measure everything in §1 before starting.*
