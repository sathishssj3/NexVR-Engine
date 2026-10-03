# Stereix Engine — Attacker-Perspective Security & Reliability Audit

**Date:** 2026-10-03 · **Scope:** Read-only audit (no code changed) · **Method:** Red-team sweep across 4 attack surfaces: (1) C++ injector/DLL chain, (2) graphics/rendering runtime, (3) Electron launcher + OTA, (4) backend/CI/OTA pipeline + repo secrets scan.

**Totals: 4 CRITICAL · 7 HIGH · 15 MEDIUM · 15 LOW · verified-good posture confirmed where documented.**

---

## 🔴 CRITICAL

### C-1. SHA-256 verification of `vrinject.dll` is non-enforcing — injection proceeds on hash mismatch
**File:** `stereix-client/src/injector/main.cpp:557-561`
```cpp
std::wstring computedHash = ComputeFileHashSHA256(dllPath);
if (computedHash.empty() || computedHash != EXPECTED_DLL_HASH) {
    PrintInfo("[INFO] DLL hash differs from build baseline (OTA hotfix / updated engine). Proceeding with verified PE image.");
}
```
- The check never blocks. Any x64 DLL with an `MZ` header passed via `--dll` is injected. The only enforced validation is `MZ` — and that check is **silently skipped** if `fopen_s` fails (no `else`).
- Four separate file opens (attributes `:539`, MZ read `:545-555`, hash `:558`, injection `:567`) with no exclusive handle → full TOCTOU: the file can be swapped between every check and `CreateRemoteThread`.
- Contradicts AGENTS.md, which claims the CLI "verifies the target's vrinject.dll against a SHA-256 hash header".
- **Fix:** Hard-fail on mismatch unless an explicit logged operator flag is passed; hold a `FILE_SHARE_READ` handle from hashing through injection; make the MZ check fail closed.

### C-2. `d3d11_proxy.cpp` is a 0-byte file — a broken proxy DLL is built and signed
**File:** `stereix-client/src/proxy/d3d11_proxy.cpp` (0 bytes), `stereix-client/CMakeLists.txt:78-89, 91-101`
- The build produces a `d3d11.dll` with **zero exports**, deliberately dropped next to game executables (System32 load precedence). Any game importing `d3d11.dll!D3D11CreateDevice` fails to start. The "Code-sign d3d11.dll" section signs an empty DLL. Likely accidental truncation (cf. `src/core/logger.h.bak`).
- **Fix:** Restore forwarding implementation mirroring `dxgi_proxy.cpp`; add a build-time check that the proxy exports the expected symbol set.

### C-3. Injector origin restriction is bypassable, and the privileged file-deploy runs *before* authentication
**Files:** `stereix-client/src/injector/main.cpp:310-409` (deploy first), `:412-417` (token), `:433-452` (parent check)
- Token is presence-only — the value is never validated: any process sets `STEREIX_AUTH_TOKEN=1` and passes.
- Parent allowlist matches substrings including `cmd`, `powershell`, `node`, `svchost`, `consent`; the check is **silently skipped** when `OpenProcess` fails (no `else` → abort).
- The `--copy-src`/`--copy-dst` deploy phase (clobbers files in the target dir via `MoveFileA`→`.old` + `CopyFileA`) runs **before** the token check. Via the launcher the whole CLI runs inside elevated PowerShell (parent `consent.exe` — allowlisted), so an **unauthenticated, elevated arbitrary-file-write primitive** exists.
- **Fix:** All origin checks (token value = launcher nonce, fail-closed parent check, AC tripwire) before *any* file operation.

### C-4. OTA-cached binaries are never re-verified at deploy time; cached manifests trusted blindly
**Files:** `stereix-client/launcher/electron/injectionManager.ts:34-64, 763-768`; `updateManager.ts:122-134, 251-257`
- `getLocalManifest()` reads `installed_manifest.json` from `%APPDATA%` **without signature verification** (signature is checked only at download time). `pickPreferredAsset` (packaged mode) selects the `%APPDATA%` file on `mtimeMs`/semver without re-hashing.
- **Attack:** Same-user malware (or anyone with write access to `%APPDATA%\Stereix Engine\updates\`) plants a fake `installed_manifest.json` (`engineVersion: 99.0.0`) + malicious `vrinject.dll`/`vr-inject-cli.exe` with a fresh mtime → launcher deploys + injects it, elevated; the CLI hash pin only logs INFO (C-1). Gain: arbitrary code execution inside the game and inside the elevated injection chain.
- **Fix:** Re-run `verifyManifestSignature` + re-hash every cached file against the signed manifest before `pickPreferredAsset` can select it; delete the cache on failure; make the CLI fail-closed.

---

## 🟠 HIGH

### H-1. Proxy `dxgi.dll` chain-loads `vrinject.dll` with no integrity check — durable persistence vector
**File:** `stereix-client/src/proxy/dxgi_proxy.cpp:153-162`
```cpp
strncat_s(szPath, "vrinject.dll", _TRUNCATE);
LoadLibraryA(szPath);
```
- Attacker with write access to a game directory (typical for moddable titles) replaces `vrinject.dll` once → the proxy silently auto-loads it at **every game launch**, with game privileges, no hash/signature check, no CLI involvement. Combined with C-1 the whole "verified" chain is unverified end-to-end.
- **Fix:** Verify SHA-256/Authenticode of `vrinject.dll` before chain-loading; fail closed.

### H-2. `VulkanDispatchTable::GetDeviceDispatch` returns interior map pointers — rehash UAF on render threads
**Files:** `stereix-client/src/rendering/vulkan/vulkan_dispatch_table.cpp:184-194` (readers), `:171-172, 226-228` (writers)
- `GetDeviceDispatch` returns `&it->second` and the lock is released; every per-frame hook callback then uses the pointer. A concurrent `RegisterDevice` (auto-triggered from `Hooked_vkGetDeviceProcAddr`) rehashes the `unordered_map`, dangling all outstanding pointers → 0xC0000005/heap corruption. The codebase already documented this failure class (`vulkan_layer.cpp:56-64`) but fixed only the layer-local maps.
- **Fix:** Return `std::shared_ptr<const DeviceDispatchTable>` (store `shared_ptr` values in the map) or return by value under the lock.

### H-3. Captured DX12 `CommandQueue` raw-pointer race at shutdown
**Files:** `stereix-client/src/hooks/dxgi_factory_hook.cpp:149-165`; consumer `dx11_hook.cpp:150-160`
- `GetCapturedCommandQueue()` returns a raw pointer that escapes the mutex; `Shutdown()` (`runtime_state.cpp:161`) resets the global `ComPtr` concurrently → use-after-free on a live game COM object during teardown.
- **Fix:** Return `ComPtr<ID3D12CommandQueue>` by value; drain in-flight present callbacks before reset.

### H-4. Injector frees remote path memory while a timed-out remote thread may still be executing
**File:** `stereix-client/src/injector/main.cpp:236-239 → 249`
- On `WAIT_TIMEOUT` the remote `LoadLibraryA` may still be reading the path string from `remoteMem`; `VirtualFreeEx(MEM_RELEASE)` decommits it → in-target access violation (crash-on-demand primitive against any targeted process).
- **Fix:** Deliberately leak `remoteMem` on timeout (documented) or confirm thread completion (`GetExitCodeThread != STILL_ACTIVE`) before freeing.

### H-5. DX11 hook-target validation is far weaker than DX12's
**Files:** `dx11_hook.cpp:435-445` vs hardened `dx12_hook.cpp:553-596`
- DX12 requires the vtable target to be `d3d12/d3d12core/dxgi/d3d11.dll` **and** inside `GetSystemDirectoryA` — explicitly to block proxy DLL hijacks. DX11 hooks any module-owned vtable, including a planted proxy DLL in the game directory. The most common fallback path is open to the exact hijack class DX12 defends against.
- **Fix:** Port the DX12 filename + system-directory validation into `dx11_hook.cpp::IsValidHookTarget`.

### H-6. Vulkan `Cmd*` hooks silently DROP the original driver call when device lookup fails
**File:** `stereix-client/src/hooks/vulkan_hook.cpp:481-620` (e.g. `:505-513`)
- Dispatch is keyed on the *global current device*, not the device owning the command buffer (multi-device games get the wrong driver table); when the lookup fails, `vkCmdBindDescriptorSets`/`vkCmdBeginRenderPass`/`vkCmdPipelineBarrier` etc. are dropped entirely → renderer breakage. No SEH wrapper, unlike present hooks.
- **Fix:** Track `VkCommandBuffer → VkDevice` at allocation; fail loudly on lookup failure, never silently drop.

### H-7. OpenXR frame submitter: dead code after early return — health monitor never records frames
**File:** `stereix-client/src/openxr/openxr_frame_submitter.cpp:244-258`
```cpp
XrResult res = xrEndFrame(session, &endInfo);
return res == XR_SUCCESS;          // line 245: unconditional return
if (XR_FAILED(res)) {              // lines 246-258: UNREACHABLE
    healthMonitor_->RecordFrameDropped(); ...
}
auto endTime = ...;                // latency measurement + RecordFrameSubmitted also dead
```
- `RecordFrameDropped`/`RecordFrameSubmitted` and latency measurement never execute → the OpenXR health monitor's perf/drop observability is silently broken (dead code + broken metrics).
- **Fix:** Restructure to a single exit path: `if (XR_FAILED(res)) { RecordFrameDropped(); return false; } RecordFrameSubmitted(latency); return true;`

---

## 🟡 MEDIUM

| # | Finding | Location | Scenario |
|---|---------|----------|----------|
| M-1 | `LoadLibraryA` + `MessageBoxA` inside DllMain (loader-lock violations; modal blocks under loader lock) | `dxgi_proxy.cpp:41, 146-149, 161` | Deadlock if loaded DLL's DllMain waits; hangs fullscreen/services. Defer loads to first forwarder call; log-only failure |
| M-2 | Condition-variable wait under loader lock in detach | `dllmain.cpp:28-31`, `runtime_state.cpp:38-47` | Loader-lock hazard every game exit (mitigated by module PIN). Signal `Stopping` and return immediately |
| M-3 | `srgbCorrection` defaults to `true` in-DLL — contradicts documented system-wide default `false` (BUG-17/21 regression class) | `config_manager.cpp:115`, `config_manager.h:18` | Every game without explicit config gets double gamma. Change both defaults to `false` |
| M-4 | `shaderDir`/`modelDir` from attacker-writable `vrinject.json` → arbitrary GPU code loaded (`.cso` unverified, no traversal check) | `config_manager.cpp:125-126`, `comfort_guard.cpp:24-53` | Planted config loads attacker compute shader into game device. Resolve against module dir only; hash-verify assets |
| M-5 | AC blocklist misses `BEService_x64.exe` (BattlEye's current service name) | `injector/main.cpp:457-465` | Injection into BattlEye titles proceeds while its service runs (Tier-3 fence gap). Match `beservice` prefix; re-check in DLL |
| M-6 | Telemetry opt-in gate trusts raw profile JSON, not user consent; user opt-in stored only in localStorage (functional no-op) | `telemetryManager.ts:63-69`, `injectionManager.ts:520-529`, `SettingsView.tsx:92` | Planted `telemetryOptIn:true` re-enables uploads against opt-out. Read consent from persisted settings; strip from profile merges |
| M-7 | Launcher `config:write` destroys 21 C++-side fields (BUG-06 class); `aiInpainting`↔`enableNeuralInpainter` name mismatch (toggle never read by engine) | `configManager.ts:374-391` vs `config_manager.cpp:105-160` | Every settings save wipes in-game HUD/comfort/IPD tuning. Read-modify-write on disk + alias the field |
| M-8 | file:// senders accepted by IPC allowlist + production navigation permits file:// → full preload bridge (`inject:deploy`, `config:write`, `inject:uninstall`) exposed to any local HTML page | `utils.ts:13-17`, `main.ts:175-180` | Malicious downloaded HTML navigates to file://, inherits bridge, triggers injection/config rewrite. Remove file:// from both allowlists |
| M-9 | Command injection via cmd `%VAR%` expansion in custom game launch (`exec` runs via cmd.exe; `%` legal in NTFS names) | `injectionManager.ts:362-365` | Custom game path with `%COMSPEC:x=evil%`-style substrings executes commands in launcher context. Use `shell.openPath` or `execFile` |
| M-10 | Renderer OS sandbox disabled (`no-sandbox`, `sandbox: false`) | `main.ts:33-35, 114` | Combined with M-8 the preload bridge is the whole compromise surface. Retest on Electron 42, re-enable |
| M-11 | Refresh-token rotation race: read-then-revoke non-atomic — concurrent refreshes with the same token both pass | `stereix-backend/src/services/auth/auth.service.ts:75-98` | Token-reuse window on stolen refresh tokens. Atomic conditional update (`revoked: false` in `where`) |
| M-12 | `/api/report` POST unauthenticated, no rate limit, CORS `*` — unauthenticated KV write flood evicts legit reports from the 40-entry `REPORTS_INDEX` ring buffer | `stereix-docs/landing-page/functions/api/report.js:35-124` | Spam amplifier + report eviction. Add Turnstile/rate limit per IP; HMAC-sign client reports |
| M-13 | signtool PFX password on the command line (WMI-enumerable; CI logs) | `stereix-client/CMakeLists.txt:96, 107, 157, 206` | Use `/csp` key-container or `@responsefile`, never `/p` inline |
| M-14 | `ResolveRIP` direct dereference after `VirtualQuery` — TOCTOU, no SEH (everything else uses `seh::SafeReadMemory`) | `pointer_chain_resolver.cpp:27-42` | Page freed between check and deref → in-target AV. Use `seh::SafeReadMemory` |
| M-15 | DX11 hook callbacks lack SEH guards and null-check `SubsystemContext` accessors (runs bare vs SEH-wrapped Present hooks) | `dx11_hook.cpp:280-339` (`hkCreateTexture2D:285`, `hkOMSetRenderTargets:298`, `hkClearDepthStencilView:312`, `hkUpdateSubresource:331`) | Teardown-ordering window → in-flight callback derefs destroyed collector. Null-check + `__try/__except` |

---

## 🟢 LOW

| # | Finding | Location |
|---|---------|----------|
| L-1 | x64 `HMODULE` truncated to 32-bit exit code → successful load looks like failure (spurious retry) | `injector/main.cpp:221-224` |
| L-2 | Double `MH_Initialize()` call works by accident (`ALREADY_INITIALIZED` dependent) | `input_hook.cpp:161`, `dx12_hook.cpp:383` |
| L-3 | `HookedGetRawInputData` writes `*pcbSize` without null check | `input_hook.cpp:109-111` |
| L-4 | Target-PID heuristic fails open (memory sanity check skipped on `GetProcessMemoryInfo` failure) | `injector/main.cpp:500-511` |
| L-5 | PageScanner sweep wipes the fast-path cached candidate (instant-lock lost after ~4 s) | `page_scanner.cpp:31-52 vs 300-304` |
| L-6 | AC tripwire races game launch; injected DLL never re-checks | `injector/main.cpp:466-484` |
| L-7 | `QueueWaitIdle` twice per present + `vkQueueWaitIdle` naive sync — serializes GPU against the 11.1 ms budget | `vulkan_layer.cpp:352-362`, `vulkan_renderer.cpp:353` |
| L-8 | Dead code: `ourSemaphore` never created (semaphore-swap branch unreachable); `logger.h.bak` dead backup in source tree | `vulkan_layer.cpp:322, 388-392`; `src/core/logger.h.bak` |
| L-9 | `g_dx12MappedResources` grows unbounded (games releasing without Unmap) | `dx12_hook.cpp:90-115` |
| L-10 | `GetRealProc` lazy caching freezes `nullptr` forever if first call races init | `dxgi_proxy.cpp:48-51, 63-131` |
| L-11 | PII sanitizer gaps: forward-slash paths, `%USERPROFILE%`, `~`, public IPv4/IPv6 not redacted before Discord/KV upload | `telemetryManager.ts:40-50` |
| L-12 | Release-uploader GitHub token visible as curl argument (process command line) | `scripts/upload_to_releases_repo.ps1:78-87` |
| L-13 | Unpinned Vulkan SDK "latest" download + `npm install` (lockfile-bypassing) in CI — supply-chain drift | `.github/workflows/package-setup.yml:64,115`, `release.yml:91,221,227,359`, `sdk-ci.yml:70` |
| L-14 | BUG-19/20 residue: dead ternary (identical branches), OTA profiles still precede local in DEV, OTA shaders win unconditionally in packaged mode | `injectionManager.ts:504-506, 438-441`, `configManager.ts:231-241` |
| L-15 | `show-password.js` prints plaintext admin password from `_admin-password.txt` on disk | `stereix-docs/landing-page/scripts/show-password.js` |

---

## ℹ️ INFO — Verified Good (attacker tried, code won)

- **Command injection in launcher properly mitigated:** `-EncodedCommand` base64 + `escapePs` single-quote escaping + `execFile` args-array + `Number.isSafeInteger` PID validation (`injectionManager.ts:801-834, 67, 77`; `diagnosticsManager.ts:158-162`).
- **Electron core hardening correct:** `contextIsolation: true`, `nodeIntegration: false`, `webSecurity: true`, trusted-sender checks, allowlisted `openExternal`.
- **Renderer XSS clean:** no `dangerouslySetInnerHTML`; all game names/paths/log lines render as React text nodes.
- **OTA crypto strong at download time:** `updates/manifest.json` carries a valid **Ed25519 signature** over a canonicalized blob (verified live against pinned key); private key gitignored and untracked in git history (`git ls-files` clean of `.pem`/`.key`/`.env`); all 25 assets hash-covered, HTTPS-only; `resolveWithinRoot` blocks manifest-path traversal.
- **Backend fail-closed in production:** `config.ts` superRefine rejects default JWT_SECRET and requires ≥32 chars in production; helmet + CORS allowlist + rate limit on `/api/`; Prisma `$queryRaw\`SELECT 1\`` parameterized; `fetch_reports.mjs` env-based, refuses default credentials.
- **No committed secrets:** repo-wide scan found no live credentials; helm values use placeholder AWS account `123456789012`; `report.js:186` `nexvr_admin_telemetry_secret_2026` is a deny-listed (burned) old key, correctly rejected — but its value remains public in git history.
- **AGENTS.md posture respected in runtime:** BUG-10 probe (`dx11_hook.cpp:128-129`), BUG-11 monoscopic fallbacks in all three backends, BUG-12 `vkCmdCopyImage` (blit only in dispatch-table binding), BUG-13 `ReleaseSwapchainReferences` on both DX11/DX12, BUG-15 subImage zero-init, BUG-22 lock hysteresis (`camera_delta_tracker.cpp:180`), BUG-23 `effectiveSyncInterval = 0` (`dx11_hook.cpp:216, 235`), CLI `asInvoker` + UAC-consent elevation, AC tripwire with no Tier-3 bypass logic.
- **Shader gamma rule compliant:** `ApplySrgbTransfer` gated on `SrgbCorrection != 0` constant-buffer flag (opt-in) — though it contradicts BUG-21's "completely removed" doc claim (doc/code drift to reconcile).
- **Legacy `nexvr-client/launcher/`** contains only stale build output (11.2 MB asar, Sep 28) but remains a CI publication fallback (`release.yml:379`) — a stale launcher could be published as a release. Delete it or remove the fallback. `scripts/injector_loop.ps1` is an unbounded `while($true)` injector with a hardcoded game name — violates QUAL-01/QUAL-04; delete.

---

## 🎯 Attack-Surface Summary (how "I" would break in)

1. **Easiest path (same user):** spawn `vr-inject-cli.exe` via `cmd.exe` (parent allowlist accepts it) with any non-empty `STEREIX_AUTH_TOKEN` → generic, integrity-unchecked DLL injector (C-1/C-3). Or drop a fake `installed_manifest.json` + malicious DLL into `%APPDATA%\...\updates\` and let the launcher deploy + inject it elevated (C-4).
2. **Durable path (game-dir write):** replace `vrinject.dll` once → `dxgi.dll` proxy auto-loads it at every game launch, verified by no one (H-1).
3. **Local page path:** any downloaded HTML navigates to `file://` → inherits the full preload bridge → inject/config-write/uninstall without consent (M-8), with the renderer sandbox off (M-10).
4. **Crash-for-hire:** target any game and free remote memory under a timed-out injection thread (H-4), or trigger the Vulkan dispatch-table rehash UAF from a game thread (H-2).
5. **Cloud path:** flood `/api/report` to evict real telemetry (M-12); the burned admin key in git history confirms rotation discipline is needed (INFO).

**Priority remediation order:** C-1 → C-3 → C-4 → H-1 → M-8 → C-2 → H-2/H-3/H-4/H-7 → M-3/M-7 (config drift) → rest.
