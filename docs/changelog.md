# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]
*Active development changes that have not yet been packaged into an official release.*

## [0.2.1] - 2026-09-15

### Added
- **Unity B2B Adapter Depth Buffer & Reverse-Z Submission**:
  - `nexvr_unity_bridge.cpp`: Exported `NexVR_Unity_StageFrameWithDepth` and `NexVR_Unity_SupportsDepthSubmission`.
  - Maintained `NexVR_Unity_StageFrame` as backward-compatible forwarder delegating with null depth (DEC-020 compliance).
  - Staged depth texture, nearZ, farZ, and depth range into lock-free ring buffer across Game Thread and Render Thread.
  - `NexVR.cs`: Added `NexVRDepthRange`, `DepthInfoDX11`, and P/Invoke bindings for SDK and bridge depth functions.
  - `NexVRCamera.cs`: Added depth buffer submission via `RenderTexture.GetNativeDepthBufferPtr()` and automated Reverse-Z detection via `SystemInfo.usesReversedZBuffer`.
  - Added dedicated test suite `tests/test_unity_bridge.cpp` verifying exports, ring buffer boundaries, slot release, detached resets, stats, and thread-safe staging/consumption under concurrent load (10/10 tests passing).

## [0.2.0] - 2026-09-15

### Added
- **B2B SDK Depth Buffer Submission (`XR_KHR_composition_layer_depth`)**:
  - Added `NexVR_SubmitFrameWithDepthDX11`, `NexVR_SupportsDepthSubmission`, `NexVR_DepthInfoDX11`, and `NexVR_DepthRange`.
  - Enables accurate positional time-warp and occlusion against runtime-drawn VR overlays.
  - Full native Reverse-Z handling (`nearZ > farZ`, `NEXVR_DEPTH_RANGE_REVERSED`) without artificial clip plane inversion.
  - Synchronous shared-lock protection (`ID3D11Multithread`) encompassing both color and depth texture copies.
  - Pre-allocated chained view depth info vectors to eliminate dangling raw pointer hazards.
  - Verified live against SteamVR (90/90 frames with depth, 0 D3D11 debug errors).
- **Additive ABI Evolution Policy (`DEC-020`)**:
  - Established rule that SDK additions must always be additive via new exported entry points.
  - Existing signatures (`NexVR_SubmitFrameDX11`) remain byte-identical and forward internally.
  - Struct size validation (`structSize`) enforced on all entry points.
- **Locate-Once View Pose Caching (`DEC-016`)**:
  - `WaitFrame` caches view poses against frame tokens, guaranteeing identical poses between game rendering and compositor reprojection.
- **Null-Texture Frame Skip Contract (`DEC-017`)**:
  - Allowed `NULL` color texture submit on skipped frames to maintain clean OpenXR frame pairing without dummy allocations.
- **Launcher Security Boundaries (`DEC-018`, `DEC-019`)**:
  - Zero-argument IPC contracts for `dialog:pickGame`, `session:preflight`, and `launch:start` preventing arbitrary elevated command execution.
  - Unelevated anti-cheat preflight check (`vr-inject-cli --check`) reading `SEC-01` single-source signal table.
  - Mandatory preflight re-check immediately prior to UAC elevation.
- **Launcher Cyberpunk Glassmorphic UI**:
  - State-of-the-art UI redesign with high-contrast diagnostic indicators and animated pipeline stage visualizer.

### Added
- **MVP-002 — OpenXR integration. The game now appears in the headset.** `vrinject.dll` creates an OpenXR
  instance at start-up, waits for the game's own D3D11 device to surface through the `Present` detour, binds
  a session to it, and from then on submits a frame to the compositor on every single presented frame.
  Validated live in a headset against a hardware D3D11 target.
- **Flat passthrough before stereo lock.** Until camera matrices and depth are locked — menus, loading
  screens, and all of today's gameplay — the game's backbuffer is copied into both eye images rather than
  nothing being submitted. A headset showing the runtime's void reads to users as a crash; a flat image
  reads as software that is working. **Known gap:** the copy is currently an unscaled crop, because D3D11
  offers no scaling copy and a resampling blit needs shaders. Phase 2 replaces it.
- **HMD pose tracking.** Head position and orientation come from `xrLocateViews` each frame and are
  submitted as a stereo projection layer, so the image is stabilised against head motion by the compositor.
  Both the position and orientation validity flags are checked, not just the call's result — an invalid pose
  submitted as though it were valid produces a warped image that reads as a rendering defect.
- **Eye swapchains negotiated against the runtime, not assumed.** Format is chosen from what the runtime
  offers, preferring sRGB over our own linear options so the image is not double gamma-corrected; size comes
  from the runtime's recommendation (1748×1944 per eye on SteamVR here) rather than the game's window.
- **OpenXR SDK added to the zero-setup build.** `cmake -B build -S . -A x64` now also fetches
  `KhronosGroup/OpenXR-SDK`. Still no vcpkg, no Conan, no manual SDK downloads.
- **12 GoogleTest suites, 219 cases.** The OpenXR suites deliberately do **not** start the compositor by
  default: creating a session launches SteamVR and, on a machine with no headset, an error dialog. Session
  coverage is opt-in via `NEXVR_TEST_OPENXR_SESSION=1`, so `ctest` stays safe to run on any machine.
- **MVP-001 — Core DXGI swapchain hooking.** `vrinject.dll` now attaches to a running DirectX 11 game,
  detours `IDXGISwapChain::Present` and `::ResizeBuffers`, and reports what it sees. Validated end-to-end
  against a live hardware D3D11 target.
- **`vr-inject-cli.exe`** — the universal injector. Targets a process by `--pid` or `--exe`, confirms the
  DLL actually mapped rather than trusting the loader's return value, and explains every failure in terms
  the operator can act on. Distinct exit codes (0 success, 1 injection failed, 2 usage, 3 anti-cheat
  refusal) so the launcher can react without parsing text.
- **Anti-cheat refusal gate.** Before opening a target, the injector checks for EasyAntiCheat, BattlEye,
  Vanguard, and Ricochet across sibling files, running processes, loaded kernel drivers, and the target's
  own modules. A detection is a refusal, not a warning, and there is deliberately **no override flag**.
- **Capability registry** — a lock-free record of which engine capabilities are live, so downstream
  subsystems can gate on real state instead of hoping. This is the piece MVP-002's OpenXR work gates on.
- **Configuration via `vrinject.json`** — log level and destination, DXGI hook toggle, and diagnostics
  sampling. Unknown keys are preserved across a save so the launcher and the engine cannot clobber each
  other's settings; bad values produce a warning naming the offending value rather than a silent default.
- **Diagnostic log** at `%LOCALAPPDATA%\NexVR\logs\vrinject.log`, mirrored to the debugger. Lines are
  atomic and flushed individually, because the last line before a crash is the one that matters.
- **Zero-setup build.** `cmake -B build -S . -A x64` fetches MinHook, nlohmann/json, and GoogleTest. No
  vcpkg, no Conan, no manual SDK downloads.
- **`d3d11_test_app.exe`** — a deterministic injection target with a real window, a real hardware
  swapchain, and a `Present` loop. Makes hook validation repeatable without a game, and faithfully
  reproduces a game's own resize behaviour so a regression on our side fails loudly on its side.
- **9 GoogleTest suites, 171 cases**, plus GitHub Actions CI that configures, builds Release, and runs them
  on every push.

### Changed
- **The `Present` detour now does VR work, not just bookkeeping.** It captures the game's D3D11 device on the
  first frame and drives one OpenXR frame per presented frame. Everything on that path is bounded: the
  swapchain image wait has a finite timeout rather than blocking indefinitely, and no exception is allowed to
  escape back into the game's render loop.
- **Engine teardown now stops VR before it removes the hooks.** The frame path runs *inside* the `Present`
  detour, so tearing the detour out first would leave a frame in flight against subsystems that were already
  gone.

### Deprecated
- 

### Removed
- 

### Fixed
- Diagnostics that are already rate-limited now log at a level visible by default. The per-frame `Present`
  line was sampled to roughly one frame in 300 *and* gated behind `Debug`, so with default configuration
  the engine's primary "hooks are live and frames are flowing" signal produced nothing at all. Two guards
  where one was needed, and the second cancelled the first. (BUG-07)

### Security
- **The injector refuses to inject into anti-cheat-protected targets, with no way to override it.** Injecting
  a DLL and detouring graphics calls is indistinguishable from cheating; proceeding risks a permanent ban on
  the user's account and, for kernel-mode anti-cheat, can bugcheck the machine. Detection is a refusal list
  that fails closed, never an allowlist of "known safe" games, so an anti-cheat nobody has catalogued yet is
  still caught. (SEC-01)
- The project will not implement driver-level hiding, module unlinking, signature spoofing, or any other
  technique whose purpose is evading detection. If a feature can only be delivered that way, the title is
  unsupported. (SEC-02)
- The injector no longer requests administrator rights in its manifest. It needs elevation to inject, but
  demanding it up front meant the process refused to start from a terminal — hiding `--help` and every
  diagnostic behind a generic Windows elevation error. It now runs, tries, and explains. (QUAL-04)

---

## [1.0.0] - 2026-08-17
*Initial release of the system after executing the PSB setup phase for NexVR Engine.*

### Added
- Complete **PSB (Plan, Setup, Build) system** configuration templates for NexVR Engine.
- Core workspace memory file (`claude.md`) with VR injection regression prevention rules.
- High-level engineering roadmap (`project-status.md`) for MVP and milestone tracking.
- System design specs and C++/Electron architecture rules (`architecture.md`).
- Unified team onboarding playbook (`team-onboarding-playbook.md`).

---

## Team & Claude Update Protocol

To keep this ledger accurate and useful for both human developers and AI sessions:

1. **Keep it High-Level:** Unlike raw Git commit logs, the changelog should focus on user-facing value and architectural milestones.
2. **Draft Before Merge:** Ensure the `[Unreleased]` section is updated on your feature branch *before* submitting a Pull Request to `main`.
3. **Tag Releases:** When merging to `main` and deploying, move current `[Unreleased]` bullet points under a new version header with the current date.
