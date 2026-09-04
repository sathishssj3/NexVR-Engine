# NexVR Engine — Master Execution Prompt: Scalability & Maintainability

**Purpose:** A self-contained brief. Paste into a fresh Claude Code session, or hand to a subagent, and it has everything needed to execute without re-deriving context.
**Baseline measured:** 2026-08-16 23:15 IST, after the `src/rendering/{dx11,dx12,vulkan}/` + `src/heuristics/` reorganization.
**Companion:** [`MASTER_PROMPT_TO_100.md`](./MASTER_PROMPT_TO_100.md) — scored against a rubric. This document is governed by a different question. See §7.

---

## 0. Role & Governing Principle

You are a **Principal C++ Software Architect** working on NexVR Engine — a universal VR injection layer (DirectX 11/12, Vulkan → OpenXR), C++20, Windows x64 MSVC.

Your mission is not to make the code "clean." It is to make **the next change cheap**.

> **Maintainability is one measurable question: what does it cost to add the next thing?**

Not an aesthetic. Not a principle you argue about in review. A number you can print. Every task in this document either lowers a measured cost or installs a ratchet that stops it from rising again.

### The rule that makes this stick

**Discipline does not scale. Ratchets do.**

Between 22:00 and 23:15 on the baseline day, during a reorganization explicitly aimed at improving structure, the singleton count went from **36 to 39**. Nobody decided to add coupling. Three hours of good-faith structural work made it worse anyway, because `::Get()` is the path of least resistance and nothing was watching.

That is the whole argument. A rule in a document is a hope. A rule in CI is a fact. §4 is the most important section here.

---

## 1. Measured Cost-to-Change

Re-measure with §4's commands before starting. These are the baseline.

| Operation | Cost today | Target |
|:---|:---:|:---:|
| Add a 4th graphics backend | **20 files** (7 outside `src/rendering/`) | 1 directory + 1 registration |
| Add AI model #3 of 7 planned | ~4 files, but `StereoPipeline` names concretes | 1 file, registered |
| Add a per-game compatibility entry | 1 hardcoded `if`, recompile | 1 JSON file, no rebuild |
| Add a config field | 2 files, hand-synced, silent drift | 1 schema, both sides generated |
| Reason about a subsystem's blast radius | unbounded — **39** `::Get()` singletons | explicit, declared |

The last row is the reason the others are hard. Any code can reach any subsystem, so nobody declares a dependency and nobody can see one.

---

## 2. What Is Already Right — Protect It

Do not "improve" these. They are load-bearing.

| Asset | Where |
|:---|:---|
| `IGraphicsBackend` with `CreateOpenXRSession` / `SubmitStereoFrame` | `src/rendering/igraphics_backend.h:59` |
| `IAIBackend` — the AI abstraction already exists | `src/ai/backend/ai_backend.h:30` |
| `IRenderer` | `src/rendering/irenderer.h` |
| `Prepare → Validate → Install → Verify → Commit` + `m_rollbackStack` | `src/core/hook_manager.h` |
| Documented non-nested lock ordering; next-layer pointers resolved before lock | `src/hooks/vulkan_layer.cpp:151–176` |
| `SafeReadMemory` zeroes destination on AV | `src/core/seh_shield.h` |
| `LocateViews` validity gate + last-good fallback | `src/openxr/openxr_frame_submitter.cpp:14–40` |
| Per-backend directories, `src/heuristics/`, `graphics_types.h` extracted | the recent reorg — right direction |
| graphify + `project_memory.md` + brain artifacts | knowledge system |

The reorg grouped code physically. That is necessary and not sufficient: `openxr_swapchain_manager.cpp` still branches on the backend enum. **Physical structure and logical coupling are independent variables**, and only the second one sets cost-to-change.

---

## 3. The Six Levers, In Order

Levers 1 and 2 make the other four cheap. Do not reorder.

---

### LEVER 1 — `SubsystemContext`: make reachability explicit

**Current:** 39 Meyer's singletons (`static T& Get()`), up from 36 during a structural refactor.

**Why it is the master lever.** Three consequences, all compounding:

1. **Untestable in isolation.** Instantiating one subsystem drags in the graph. This is why 73 tests exist and none exercises the assembled pipeline.
2. **Unbounded blast radius.** `graphify` shows the edges that exist; it cannot show which edges are *permitted*. There is no such thing as an unpermitted edge today.
3. **Nondeterministic teardown.** Meyer's singletons destruct in reverse order of first construction — an order set by runtime call sequence, which varies per game, per injection timing, per API. `VulkanDispatchTable` can be destroyed while `VulkanLifecycleManager` still holds handles routed through it.

**Target.** A `SubsystemContext` owning every subsystem, constructed in `RuntimeState::BackgroundInitialize`, passed by reference. Migrate incrementally — each subsystem moved is independently shippable.

**The point is not the refactor.** It is that a subsystem absent from the context becomes *unreachable*. Coupling stops being a code-review request and becomes a compile error.

**Acceptance:** singleton count strictly decreasing, verified by §4.1 on every commit. Destruction order visible in one file.

---

### LEVER 2 — Test seams: the headless harness

**Current:** every test is a unit test. Nothing exercises Golden Pipeline stages 4→8. The contract in `docs/GOLDEN_PIPELINE.md` has no executable enforcement.

**Why it is second and not fifth.** You cannot safely change what you cannot verify. Lever 1 is frightening today precisely because nothing proves the pipeline still works afterward. Build the harness and every subsequent lever becomes mechanical.

**Target.** A mock OpenXR runtime plus a synthetic present loop per backend, running in CI with no headset. Minimum surface: `xrCreateInstance`, `xrGetSystem`, `xrCreateSession`, `xrCreateReferenceSpace`, `xrEnumerateSwapchainFormats`, `xrCreateSwapchain`, `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`, `xrAcquire/Wait/ReleaseSwapchainImage`, `xrEndFrame`.

**Feed scripted head poses.** A scripted 90° left rotation with an asserted view-matrix delta catches an inverted composition sign in CI forever, instead of once, manually, on someone's face.

**Acceptance:** at least one CI test drives stages 4→8 end to end. Deliberately invert one condition in the stereo path and confirm it goes red.

---

### LEVER 3 — Get the backend enum out of non-rendering code

**Current:** 20 files reference `GraphicsBackend::`; **7 are outside `src/rendering/`** — `compatibility_scorer.cpp`, `frame_coordinator.cpp`, `graphics_types.h`, `openxr_runtime_manager.cpp`, `openxr_swapchain_manager.cpp`, plus `src/heuristics/`.

Those files should not know DX12 from Vulkan. A compatibility scorer scores capabilities; a swapchain manager manages swapchains.

**Target.** Every API-specific decision behind `IGraphicsBackend`. `graphics_types.h` may define the enum — nothing outside `src/rendering/` may branch on it.

**Acceptance:** §4.2 returns 0. Adding a backend touches one directory plus one registration line.

---

### LEVER 4 — Data-driven game profiles

**Current:** one hardcoded check, `src/core/engine_detector.cpp:83`:

```cpp
if (exeStr.find("HogwartsLegacy") != std::string::npos) {
```

At one it is pragmatic. At fifty, `engine_detector.cpp` is unreviewable and every supported game ships a recompile. `project_memory.md` §6 already prohibits this pattern.

**Migrate now, while it costs one function.** Versioned JSON profiles: executable match, engine hint, camera/depth heuristic tuning, anti-cheat tier, compatibility notes.

**Second-order benefit:** the catalog becomes data other people can contribute. Your scorecard's launch gate wants 10 verified titles — that is a community task with profiles and a solo task without them.

**Acceptance:** §4.3 returns 0 game-name string literals in `src/`. Adding a title requires no rebuild.

---

### LEVER 5 — One schema source for config

**Current:** `src/core/config_manager.cpp` and the launcher TypeScript schema, manually synced. Filed as BUG-06.

**It is not a bug, it is a bug class.** They will drift again, and the failure is silent: a setting the UI shows and the engine ignores, or worse, one the engine reads as garbage. Any instruction of the form "remember to update both sides" is a defect waiting on a busy day.

**Target.** One source generating both — JSON Schema → C++ struct + TS interface, or C++ as source emitting `.d.ts` at build time. Either kills the class permanently.

**Acceptance:** a field added in one place appears on both sides with no hand edit. A deliberate mismatch fails the build, not the user.

---

### LEVER 6 — Route AI stages through `IAIBackend`

**Current:** `IAIBackend` exists at `src/ai/backend/ai_backend.h:30` — good. But `StereoPipeline` (`src/rendering/stereo/stereo_pipeline.h`, degree 31) names `NeuralInpainter` and `ComfortGuard` as concrete members at lines 69–70.

`project_memory.md` §5 plans **seven** AI models. Two concrete members is a pattern. Seven is a second God Object — on the frame path, inside an 11.1 ms budget.

**Target.** Every stage behind `IAIBackend`, registered rather than named, each declaring its budget in milliseconds and its execution path (synchronous render thread vs. async worker, per the §5 execution blueprint). The pipeline enforces the total budget and skips stages that would blow it.

**Acceptance:** adding a model touches one file plus a registration. `grep -c "Inpainter\|Comfort\|Predictor\|Synthesizer" src/rendering/stereo/stereo_pipeline.h` returns 0.

---

## 4. Ratchets — the part that actually holds

**Add these to CI before starting any lever.** Each is a one-way metric: it may improve, never regress. This is what separates a codebase that stays maintainable from one that was maintainable once.

Store current values in `docs/coupling_budget.txt` — one `key=integer` per line — and fail the build if any measurement exceeds its recorded value.

### 4.1 Singleton count — baseline 39

```bash
grep -rlE "static\s+\w+\s*&?\s*Get\(\)" src/ --include=*.h | wc -l
```

### 4.2 Backend enum leaks outside rendering — baseline 7

```bash
grep -rl "GraphicsBackend::" src/ | grep -vc "src/rendering/"
```

### 4.3 Hardcoded game names — baseline 1

```bash
grep -rniE '"(hogwarts|cyberpunk|skyrim|elden|witcher|fallout)' src/ --include=*.cpp | wc -l
```

### 4.4 Test coverage direction — baseline sources=79 registered=73

```bash
echo "sources=$(find tests -name '*.cpp' | wc -l) registered=$(ctest --test-dir build -N 2>/dev/null | tail -1)"
```

### 4.5 God-object degree — baseline FrameCoordinator 34, StereoPipeline 31

```bash
graphify explain "FrameCoordinator" | grep Degree
```

**A ratchet that blocks a legitimate change is doing its job.** The correct response is to lower the budget deliberately and record why — never to raise it quietly, and never to satisfy it by deleting the thing being measured. This project has already emptied a quarantine list by deleting all seven tests in it; the list read clean and the coverage was gone.

---

## 5. Recipes — how to extend correctly

New code follows these. Deviating requires a recorded decision.

### Adding a graphics backend

1. New directory `src/rendering/<api>/`.
2. Implement `IGraphicsBackend` — session creation, stereo submission, resource context, profiling.
3. Register in the backend factory. **One line.**
4. Nothing outside your directory learns the API exists.
5. Harness (Lever 2) gains a synthetic present loop for it.

### Adding an AI stage

1. Implement `IAIBackend`.
2. Declare budget in milliseconds and path: synchronous render thread (<1.5 ms total across all sync stages) or async worker.
3. Register. Do not add a member to `StereoPipeline`.
4. Pipeline skips it when the frame budget would be exceeded — degradation, not a dropped frame.

### Adding game support

1. New JSON profile. No C++ change, no rebuild.
2. Anti-cheat tier is a required field. Tier 3 (Vanguard, BattlEye, active EAC, Ricochet) is rejected at load, never at runtime.

### Adding a config field

1. Edit the schema. Both sides regenerate.
2. If you edited two files, you did it wrong — fix Lever 5 first.

### Adding a subsystem

1. Constructed and owned by `SubsystemContext`.
2. Dependencies arrive by constructor parameter, declared in the signature.
3. No `::Get()`. If you cannot reach what you need, that is the design telling you something — usually that the dependency belongs somewhere else.

---

## 6. Invariants — never traded for structure

A refactor that violates one of these has failed regardless of how much coupling it removed.

1. **Golden Pipeline contract.** `docs/GOLDEN_PIPELINE.md` freezes eight stages. Inputs, outputs, threading model, GPU execution order across each boundary are contractual. Move code freely; do not change what crosses a boundary or on which thread.
2. **MinHook ownership.** `HookManager` exclusively owns `MH_Initialize`/`MH_Uninitialize` (BUG-01/02).
3. **DX12 single queue.** All stereo dispatches and copies to `Dx12LifecycleManager::Get().GetMainQueue()`, for strict FIFO with game rendering.
4. **Vulkan dispatch safety.** Never return hooked pointers from `vkGet*ProcAddr` fallbacks. Untracked devices pass through or fail closed — never `VK_SUCCESS` with an uninitialized handle.
5. **Layer/detour exclusion.** `IsLayerActive()` suppresses MinHook detours when the Vulkan layer is live. Both at once corrupts the heap.
6. **Dual-mode rendering.** `shouldAttemptStereo == false` still submits to OpenXR — copy the backbuffer to both eyes. Never skip submission.
7. **Head-pose fallback.** Invalid `viewStateFlags` holds the last good pose. Never identity.
8. **DllMain is a shim.** No work, no thread creation, no waiting on threads created there (DEAD-05).
9. **Anti-cheat fence.** Tier 3 never targeted, never bypassed.
10. **No `::Get()` in new code** once Lever 1 lands in a subsystem's area.

Process: `graphify query` / `explain` / `path` before touching shared headers — never blind search-and-replace. `graphify update .` after every `.cpp`/`.h` change. No `git reset --hard`, `git clean -fd`, or `git push --force` without explicit human approval.

---

## 7. Relationship to the Score Brief

[`MASTER_PROMPT_TO_100.md`](./MASTER_PROMPT_TO_100.md) drives a rubric from 72 toward ~93. This document is governed by cost-to-change.

They overlap at exactly two points — `SubsystemContext` (its Phase 4.5, Lever 1 here) and the headless harness (its Phase 5.1, Lever 2 here). **That convergence is the useful signal**: the two highest-value items under both lenses are the same two items, which is strong evidence they are genuinely the right next work rather than artifacts of either framing.

Where they diverge, prefer this document. A rubric is a proxy; cost-to-change is the thing the proxy was standing in for. Levers 3–6 raise no score meaningfully and matter more over a two-year horizon than anything that does.

---

## 8. Reporting Format

After each lever:

```markdown
## Lever N — <name>

### Cost-to-change delta
| Operation | Before | After |
|---|---|---|

### Ratchets
| Ratchet | Budget | Measured | Direction |
|---|---|---|---|
| 4.1 singletons | 39 | <n> | ↓ / — / ↑ FAIL |
| 4.2 enum leaks | 7 | <n> | |
| 4.3 game names | 1 | <n> | |
| 4.4 coverage | 79/73 | <n>/<n> | |
| 4.5 FrameCoordinator degree | 34 | <n> | |

### Invariants Re-verified
- [ ] Golden Pipeline boundaries unchanged
- [ ] MinHook ownership exclusive
- [ ] Vulkan layer/detour exclusion intact
- [ ] DX12 single-queue contract intact
- [ ] No test deleted, no ratchet raised
- [ ] Suite green against a fresh build (timestamps checked)

### Budget changes
Any ratchet budget lowered — and why. Any raised — and the explicit decision authorising it.
```

**Report unverified work as unverified.** Not "done," not "should work." A ratchet you did not run is a ratchet that did not pass.

---

*Baseline measured 2026-08-16 23:15 IST against the working tree after the `src/rendering/{dx11,dx12,vulkan}/` reorganization, with 54 uncommitted `src/` changes present. The graphify index was stale at measurement time — `stereo_pipeline.h` had moved to `src/rendering/stereo/`. Run `graphify update .` and re-measure every ratchet in §4 before starting.*
