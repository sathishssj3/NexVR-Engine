# NexVR Engine Repository Threat Model

## Overview

NexVR Engine is a Windows desktop application that launches games, copies runtime assets into game directories, elevates a native injector, injects `vrinject.dll` into third-party game processes, hooks graphics/input APIs, and submits frames to an OpenXR runtime. Its primary runtime surfaces are:

- the Electron/React launcher under `launcher/`
- Electron main-process IPC handlers under `launcher/electron/`
- the elevated injector in `src/injector/main.cpp`
- the injected DLL and hook lifecycle under `src/dllmain.cpp`, `src/core/`, and `src/hooks/`
- Direct3D/OpenXR rendering and GPU synchronization under `src/rendering/`
- packaging and dependency acquisition in `CMakeLists.txt`, `launcher/electron-builder.config.js`, and `.github/workflows/release.yml`

The most important assets are the user's game installations, local files and diagnostic data, process integrity of launched games, administrator privileges used for injection, the integrity of packaged DLL/EXE/model/shader assets, OpenXR session stability, and availability of the host game and desktop.

## Threat Model, Trust Boundaries, and Assumptions

### Trust boundaries

1. **Renderer to Electron main process.** The renderer is less trusted than the main process. All methods exposed by `launcher/electron/preload.ts` cross into privileged IPC handlers capable of launching processes, opening URLs/files, writing game directories, exporting diagnostics, and requesting elevation.
2. **Launcher to game installation.** Steam/Epic manifests and user-selected custom executables influence paths and process selection. Game directories may contain third-party or locally modified content and should not be treated as trusted application storage.
3. **Launcher to elevated PowerShell/injector.** `launcher/electron/injectionManager.ts` builds an elevated PowerShell command that copies binaries and injects a DLL. Values crossing this boundary must be validated without relying on shell quoting alone.
4. **Injector to target process.** `src/injector/main.cpp` obtains powerful process rights and runs `LoadLibrary` remotely. Target identity, architecture, path integrity, and binary provenance are security invariants.
5. **Injected DLL to host process.** The DLL executes inside an arbitrary game process and hooks graphics, input, and focus APIs. Invalid memory access, loader-lock work, unbounded waits, or shutdown races can crash or hang the game.
6. **Game GPU to OpenXR/GPU worker queues.** Shared textures, fences, command allocators, keyed mutexes, and swapchain resources cross asynchronous GPU/CPU boundaries. Incorrect ownership or infinite waits can deadlock the render path.
7. **Local machine to build/update supply chain.** CMake and CI download third-party archives and SDK installers. Release artifacts require integrity, reproducibility, and signing controls.

### Input ownership

- **Attacker-controlled or potentially hostile:** compromised renderer content; crafted custom-game metadata files; modified Steam/Epic manifests; malicious files, symlinks/reparse points, executables, shaders, models, dumps, or logs in game directories; unexpected process names/PIDs; malformed host-process memory and graphics resources.
- **Operator-controlled:** selected custom executables, per-game configuration, injection/cancel actions, exported log content, environment variables, install locations, OpenXR runtime selection, signing credentials.
- **Developer/CI-controlled:** dependency URLs and versions, build scripts, packaged resources, model/shader artifacts, generated files, and release workflow permissions.

### Required invariants

- Privileged IPC is accepted only from the intended application frame and validates every argument.
- Paths derived from manifests, custom-game records, or game directories remain inside the intended root after canonicalization and reparse-point handling.
- Shell commands are not assembled from untrusted strings; process APIs receive structured executable/argument arrays.
- The injector targets exactly the selected non-system process and injects only an expected, verified DLL.
- Elevated credentials/tokens are not forgeable merely through inherited environment variables or permissive parent-name checks.
- Cleanup never performs blocking or complex hook/graphics work while the Windows loader lock is held.
- Threads have bounded, interruptible shutdown and synchronized shared state.
- GPU waits are bounded or recoverable, and resources are not reset/released while another queue/thread can still use them.
- Diagnostics exclude secrets and unnecessary sensitive files.
- External navigation is restricted to an explicit protocol/host allowlist.

## Attack Surface, Mitigations, and Attacker Stories

### Electron and IPC

`launcher/electron/main.ts` enables context isolation and disables renderer Node integration, denies new windows, and restricts production navigation. `launcher/electron/preload.ts` exposes a narrow API rather than raw IPC. These are useful controls, but the BrowserWindow sandbox is disabled and IPC handlers must still authenticate the sending frame and validate arguments.

Realistic attacker stories include renderer compromise invoking a privileged IPC method, a crafted URL reaching `shell.openExternal`, or malicious persisted game metadata causing unsafe process launch, file copy, diagnostic inclusion, or command construction.

### Process launch and injection

The launcher starts custom executables or game protocol handlers, polls process lists, copies runtime files, and invokes an elevated injector. The native injector performs basic argument, PE-header, architecture, and origin checks. Because injection is intentionally powerful, weak target selection, token checks, path checks, or binary verification can turn a renderer/local-file compromise into arbitrary elevated code execution or injection into the wrong process.

### Injected runtime and concurrency

The DLL creates worker threads, scans memory, hooks input/graphics APIs, and synchronizes multiple Direct3D queues. Existing use of atomics, mutexes, timeouts in some keyed-mutex operations, and a dedicated initialization thread reduces risk. Important failure classes remain loader-lock deadlocks, unsynchronized hook state, invalid host pointers, races during shutdown/reinitialization, fence waits that never complete, and GPU resource lifetime errors.

### Files, configuration, diagnostics, and privacy

Configuration writes are schema-validated and size-limited. Diagnostic lines are bounded and partially redacted. Game directories and persisted JSON remain a hostile boundary: canonical containment checks, type/schema checks, bounded recursion/file sizes, safe temporary-file handling, and minimal diagnostic collection are required.

### Build and supply chain

Dependencies are version-pinned but downloaded without repository-recorded hashes. CI downloads and executes a "latest" Vulkan SDK installer and has broad write permissions. Release binaries are optionally signed only when environment variables are present. A compromised upstream/archive, mutable installer, or CI token could produce malicious injection binaries.

### Less relevant or out-of-scope classes

Traditional multi-tenant authorization, server-side request forgery, database injection, and remote session management are not primary concerns because this is a local Windows application without a server or database. They become relevant only if future network/update/cloud features are added.

## Severity Calibration

### Critical

- Renderer, manifest, game-directory, or IPC input leading to arbitrary command execution in the elevated injection flow.
- Supply-chain compromise that silently ships a malicious signed injector or injected DLL.
- Target-selection failure permitting privileged injection into arbitrary sensitive/system processes without meaningful user consent.

### High

- Arbitrary file overwrite outside the selected game directory through path/reparse manipulation.
- Untrusted DLL/model/shader substitution that executes or influences code inside the elevated injector or target game.
- Reliable host-process memory corruption, use-after-free, or loader-lock deadlock affecting broadly supported games.
- Sensitive diagnostic export that unintentionally packages credentials or private user data.

### Medium

- Renderer-triggerable opening of dangerous external URI schemes.
- Wrong-process termination/injection caused by PID/name races.
- Unbounded CPU/GPU waits or thread joins that reliably hang a game, launcher, or shutdown.
- Persistent configuration corruption or uncontrolled resource consumption from malformed local metadata.

### Low

- Local-only crashes requiring direct modification of application-owned files.
- Excessive logging, weak redaction of low-sensitivity paths, or stale watchers that consume resources without crossing a privilege boundary.
- Build hardening and defense-in-depth gaps that require an already-compromised developer machine.
