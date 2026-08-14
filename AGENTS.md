# NexVR Engine — Agent Instructions

Universal VR injector: a C++ injection DLL + CLI (root CMake project) and an Electron/React launcher (`launcher/`, built separately). Windows-only, MSVC x64.

## Must read first

- `docs/project_memory.md` — repo-local memory with audit resolutions (BUG/DEAD/QUAL fixes) that edits must preserve. README explicitly requires reading it before architectural/feature changes.
- `docs/architecture.md`, `README.md` for the system design (sibling subsystems orchestrated by `RuntimeState`, capability registry, async diagnostics).

## Building the C++ engine

- Requires MSVC (VS 2022+), Vulkan SDK, and network at configure time — `FetchContent` downloads minhook, ONNX Runtime DirectML 1.16.3, DirectML 1.13.1, OpenXR SDK 1.0.34, nlohmann/json, GoogleTest, ImGui 1.90.5.
- Configure then build (or run `build.bat`, which sources VsDevCmd from a hardcoded VS path):
  `cmake -B build -S . -A x64` then `cmake --build build --config Release`
- All artifacts go to `build/bin/` (single dir is required so `vrinject.dll` sits next to `vr-inject-cli.exe`).
- Proxy DLLs (`build/bin/proxy/dxgi.dll`, `d3d11.dll`) are deliberately NOT in `bin/` — a past bug (0xC000007B, shadowing system DLLs) is why. Keep them in `proxy/`.
- `vr-inject-cli.exe` has a requireAdministrator manifest and is built against a DLL-hash header generated from `vrinject.dll`.
- Shaders: `shaders/*.hlsl` (entry point `CSMain`) are compiled at build time by fxc (cs_5_0), dxc (cs_6_0), glslc (SPIR-V) into `build/bin/vrinject_shaders/*_cs_dx11.h`; raw HLSL is also copied there for launcher-side dynamic compilation.
- Source globs under `src/` are plain `GLOB` (no CONFIGURE_DEPENDS): adding a new `.cpp` requires re-running `cmake -B build -S . -A x64`, not just `cmake --build`.

## Tests

- C++ gtest suites auto-register from `tests/test_*.cpp` (GLOB with CONFIGURE_DEPENDS). Run:
  `ctest --test-dir build -C Release --output-on-failure` or run a single `build/bin/test_<name>.exe`.
- Empty (0-byte) test files are auto-skipped (they would fail to link) — 12 exist today, and several suites are quarantined in CMakeLists (~line 750) because they reference removed APIs. Don't restore stubs as empty shells.
- Suites with their own `main()` are excluded from auto-registration: `test_openxr_dx12_swapchain`, `test_stereo_visual`.
- Launcher: `npm run test:static` (Playwright, file-content checks only, no browser), `npm run typecheck` (both renderer + electron tsconfigs), `npm run test:ci` = typecheck + static. `npm test` runs all specs.

## Launcher

- `cd launcher && npm install`; `npm run dev` (Vite + Electron), `npm run build` (vite + tsc), `npm run pack` (electron-builder; NSIS installer in `launcher/dist-electron/`).
- electron-builder must package every native asset the injector deploys (vrinject.dll, vr-inject-cli.exe, onnxruntime.dll, DirectML.dll, shaders, models) — `regression-static.spec.ts` enforces this.

## Conventions and gotchas

- Keep `src/core/config_manager.cpp` serialization in sync with launcher settings fields; drift is a known failure class (BUG-06).
- `HookManager` owns the MinHook lifecycle — do not add init/shutdown in individual hooks (BUG-01/02). Use `std::atomic` for shared hook globals (DEAD-02/04). `DllMain` is a shim only; init happens via `RuntimeState`, and detach uses bounded waits (DEAD-05).
- Use the unified injector in `src/injector/main.cpp`; never reintroduce injectors under `tools/` (QUAL-01). No game-specific hardcoded logic outside profiles/compat data (QUAL-04).
- `.onnx` models and `models/` are gitignored; CI generates dummy models with `generate_dummy_onnx.py`. Real ONNX weights are not in the repo.
- Unsigned injection DLLs/CLI get flagged by AV/Defender. Signing via `SIGN_CERT_PATH`/`SIGN_CERT_PASS`; the CMake signing blocks are currently commented out.
- Anti-cheat posture (Tier 1/2/3) is defined in `docs/project_memory.md` §7: never attempt to bypass active anti-cheat; strict-AC titles are unsupported.
- Perf budget: 11.1 ms frame; sync AI path on render thread < 1.5 ms, async AI on worker pool.

## CI

- `.github/workflows/release.yml`: builds Debug+Release on push/PR to `main`, runs CodeQL + non-fatal cppcheck; the release job (installer upload, tag-triggered) needs the C++ build to succeed first. DXC is downloaded from a pinned release in CI, not the Windows SDK.