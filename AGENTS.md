# NexVR Engine — Agent Instructions

Monorepo: a root CMake orchestrator builds the C++ engine from `nexvr-client/` (injection DLL + CLI + gtest suites); the Electron/React launcher lives in `nexvr-client/launcher/` (built separately); `nexvr-backend/` is a Node/TS cloud stack. Windows-only, MSVC x64.

## Must read first

- `docs/project_memory.md` — repo-local memory with audit resolutions (BUG-*/DEAD-*/QUAL-* fixes) that edits must preserve. README explicitly requires reading it before architectural/feature changes.
- `docs/architecture.md`, `README.md` for the system design (sibling subsystems orchestrated by `RuntimeState`, capability registry, async diagnostics).
- Repo is CodeGraph-indexed (`.codegraph/`): prefer `codegraph explore "<symbol or question>"` over grep when locating code.

## Layout

- Engine sources: `nexvr-client/src/` (per-subsystem dirs: core, hooks, rendering, openxr, vr, ai, runtime, ...); tests: `nexvr-client/tests/`; shaders: `nexvr-client/shaders/`; profiles: `nexvr-client/profiles/`.
- The build dir is always root `build/` (even though sources live in `nexvr-client/`); all artifacts land in `build/bin/`.

## Building the C++ engine

- Requires MSVC (VS 2022+), Vulkan SDK, and network at configure time — `FetchContent` downloads minhook v1.3.3, ONNX Runtime DirectML 1.16.3, DirectML 1.13.1, OpenXR SDK 1.0.34, nlohmann/json, GoogleTest 1.14, ImGui 1.90.5.
- Build (or run `build.bat`, which locates VsDevCmd via vswhere): `cmake -B build -S . -A x64` then `cmake --build build --config Release`.
- SPIR-V needs `dxc.exe`; if CMake warns it wasn't found, set `-DDXC_EXECUTABLE=<path>` like CI does (pinned DXC release). Without DXC, DX12 shader headers and SPIR-V are silently skipped.
- All artifacts go to `build/bin/` (single dir required so `vrinject.dll` sits next to `vr-inject-cli.exe`).
- Proxy DLLs (`build/bin/proxy/dxgi.dll`, `d3d11.dll`) are deliberately NOT in `bin/` — a past bug (0xC000007B, shadowing system DLLs for every test exe) is why. Deploy them next to the target game, never next to tests.
- `vr-inject-cli.exe` manifest is `asInvoker`; UAC elevation happens at injection time via a launcher-spawned elevated PowerShell (`Start-Process -Verb RunAs` in `injectionManager.ts`). The CLI also verifies the target's `vrinject.dll` against a SHA-256 hash header (`build/expected_hash.h`, regenerated on every DLL build).
- Shaders: `nexvr-client/shaders/*.hlsl` (entry point `CSMain`) compile at build time via fxc (cs_5_0 → `*_cs_dx11.h`), dxc (cs_6_0 → `*_cs_dx12.h`), glslc (SPIR-V → `*_cs_vk.h`) into `build/bin/vrinject_shaders/`; raw HLSL + `shaders/vulkan/` are copied there and mirrored to `build/bin/shaders/` (workaround for a hardcoded path in `dx12_renderer.cpp`).
- Source globs under `src/` are `GLOB_RECURSE ... CONFIGURE_DEPENDS` per subsystem: a new `.cpp` in an existing subdir is picked up on the next build; only a brand-new subdirectory needs a reconfigure.

## Tests

- C++ gtest suites: `ctest --test-dir build -C Release --output-on-failure`, or run a single `build/bin/test_<name>.exe`. Suites needing ONNX Runtime or a custom `main()` are registered explicitly in `nexvr-client/cmake/Testing.cmake`; everything else auto-registers from `tests/test_*.cpp` (CONFIGURE_DEPENDS — no reconfigure needed).
- Auto-registration skips 0-byte test stubs and standalone-`main()` harnesses (`test_openxr_dx12_swapchain`, `test_stereo_visual`). Don't restore stubs as empty shells.
- Launcher: `npm run test:static` (Playwright, file-content checks only, no browser), `npm run typecheck` (renderer + electron tsconfigs), `npm run test:ci` = typecheck + static. `npm test` runs all specs.
- CI also runs `scripts/check_coupling.ps1` (fatal, budgets in `docs/coupling_budget.txt`): singleton count, `GraphicsBackend::` references outside `src/rendering`, hardcoded game names, test sources / registered tests floors, and FrameCoordinator degree (needs graphify CLI, else skipped). Never satisfy a ratchet by deleting tests — lower the budget deliberately and record why (a past cleanup deleted a 7-suite quarantine and lost all coverage).

## Launcher

- `cd nexvr-client/launcher && npm install`; `npm run dev` (Vite + Electron), `npm run build`, `npm run pack` (electron-builder → NSIS + portable installers in `launcher/dist-electron/`).
- Packaging reads engine binaries from `nexvr-client/build/bin/` (`extraResources: ../build/bin`). CI junctions `nexvr-client\build` → root `build` before packing; locally run `mklink /J nexvr-client\build build` after a root build or `npm run pack` fails.
- `regression-static.spec.ts` enforces that the builder config and `injectionManager.ts` reference every native asset (vrinject.dll, vr-inject-cli.exe, onnxruntime.dll, DirectML.dll, shaders, models).
- `npm run sync:assets` recomputes SHA-256 hashes in `updates/manifest.json` and syncs shaders/profiles across dev/release/OTA staging; `sign:binaries` signs native assets; `sync:installed` patches an installed launcher.

## Backend (`nexvr-backend/`)

- Express + Prisma + Redis, jest tests. Prereqs: `npm run docker:up` (compose: Postgres/Redis/LocalStack), then `npm install`, `npm run db:push`, `npm run dev`. `npm run lint` = eslint over `src`.

## Conventions and gotchas

- Keep `src/core/config_manager.cpp` serialization in sync with launcher settings fields; drift is a known failure class (BUG-06). Per-game config fields (`engine`, `srgbCorrection`, `depthSubmission`, ...) must match on both sides.
- `HookManager` owns the MinHook lifecycle — do not add init/shutdown in individual hooks (BUG-01/02). Use `std::atomic` for shared hook globals. `DllMain` is a shim only; init happens via `RuntimeState`, and detach uses bounded waits (DEAD-05).
- Never hold persistent ComPtrs/views to game swapchain buffers across `ResizeBuffers` (BUG-13); never use `vkCmdBlitImage` for game→OpenXR copies — use `vkCmdCopyImage` (BUG-12); probe the swapchain buffer type instead of trusting a DX12 command queue's presence (BUG-10). Every `SubmitStereoFrame` needs a monoscopic fallback.
- Color space: never hardcode gamma math in shaders — `srgbCorrection` is a per-game profile flag fed via constant buffer (BUG-17/21); system-wide default is `false`.
- Use the unified injector in `src/injector/main.cpp`; never reintroduce injectors under `tools/` (QUAL-01). No game-specific hardcoded logic outside profiles/compat data (QUAL-04) — the coupling ratchet enforces this.
- Dev-mode asset precedence: local `build/bin/` must beat `%APPDATA%\NexVR-Dev\updates\` OTA caches (BUG-19/20); shaders must ship with OTA updates (`updates/manifest.json` lists them).
- `.onnx` models and `models/` are gitignored; CI generates dummies with root `generate_dummy_onnx.py` (→ `depth_inpainter.onnx`, `ui_synthesizer.onnx`). Real ONNX weights are not in the repo.
- Signing: CMake and electron-builder both sign when `SIGN_CERT_PATH`/`SIGN_CERT_PASS` env vars are set (no secrets in repo). Unsigned injection DLLs/CLI get flagged by AV/Defender.
- Anti-cheat posture (Tier 1 single-player / Tier 2 offline with AC disabled / Tier 3 unsupported) is defined in `docs/project_memory.md` ("Anti-Cheat & AV Posture Tier List"): never attempt to bypass active anti-cheat; strict-AC titles are blacklisted.
- Perf budget: 11.1 ms frame; sync AI path on render thread < 1.5 ms, async AI on worker pool.
- **Version bump rule**: every fix/feature/debug change bumps the version in `nexvr-client/launcher/package.json` and `updates/manifest.json` (keep them equal) before pushing updates to testers/releases; rerun `npm run sync:assets` to refresh manifest hashes.

## CI

- `.github/workflows/release.yml`: builds Debug+Release on push/PR to `main` (docs/** and `*.md` are path-ignored), runs CodeQL (Release only), non-fatal cppcheck, and the coupling ratchets (fatal). The tag-triggered release job (installer upload) needs the C++ build to succeed first.
