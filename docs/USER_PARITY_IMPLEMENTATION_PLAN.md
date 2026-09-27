# Implementation Plan — User Environment Parity & Diagnostic Resilience System

> **Objective**  
> Bridge the gap between developer workstations and consumer PCs by building an automated **Pre-Flight System Doctor**, **Diagnostic Bundle Collector**, **Automated Clean-Machine CI Verification**, and **Hardened Injection Staging** to eradicate the "works on my PC, fails on user PCs" syndrome.

---

## 1. System Architecture & High-Level Flow

```
+-----------------------------------------------------------------------------------+
|                            NEXVR LAUNCHER (ELECTRON)                             |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |               SystemDoctor Service (New: systemDoctor.ts)                  |  |
|  |  [1] Check VC++ 2015-2022 x64 Runtime Registry                              |  |
|  |  [2] Check Active OpenXR Runtime (Meta Quest / SteamVR / VDXR)              |  |
|  |  [3] Verify Native Asset Integrity (Hashes of vrinject.dll, DirectML, etc.)  |  |
|  |  [4] Detect Windows Defender / Antivirus Exclusion Status                   |  |
|  |  [5] Validate OS Build (Win10 1809+ / Win11 baseline for DirectML)           |  |
|  +-----------------------------------------------------------------------------+  |
|                                     │                                             |
|                     ┌───────────────┴───────────────┐                             |
|                     ▼                               ▼                             |
|           Health Passed (Green)           Discrepancy Detected (Red/Amber)        |
|                     │                               │                             |
|                     │                     Show One-Click Fix Button               |
|                     │                     ("Install VC++", "Whitelist AV")        |
|                     ▼                                                             |
|  +-----------------------------------------------------------------------------+  |
|  |              InjectionManager Hardening (injectionManager.ts)              |  |
|  |  - Pre-flight Process Elevation Check (Target PID elevated? -> RunAs UAC)   |  |
|  |  - Auto-deploy onnxruntime.dll & DirectML.dll next to Target .exe          |  |
|  |  - Verify Target Directory write permissions before injection               |  |
|  +-----------------------------------------------------------------------------+  |
|                                     │                                             |
|                                     ▼                                             |
|  +-----------------------------------------------------------------------------+  |
|  |              Diagnostics & Telemetry (diagnosticsManager.ts)                |  |
|  |  - Capture OS, GPU, Driver, OpenXR, & Sanitized Logs (PII scrubbed)         |  |
|  |  - One-Click Export: "Export Diagnostic Bundle (.zip)" to Desktop          |  |
|  |  - One-Click Edge Submit: Upload to https://nexvr-engine.pages.dev/api/report |  |
|  +-----------------------------------------------------------------------------+  |
+-----------------------------------------------------------------------------------+
```

---

## 2. Phased Implementation Roadmap

### Phase 1: Pre-Flight Environment Doctor (`systemDoctor.ts`)
**Goal:** Prevent launch and injection failures before they happen by detecting missing runtimes and bad configurations.

* **File to Create:** `nexvr-client/launcher/electron/systemDoctor.ts`
* **Checks Implemented:**
  1. `checkVcRedist()`: Query Windows Registry (`HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64` or `HKLM\SOFTWARE\Classes\Installer\Dependencies\VC,redist.x64,amd64*`). If missing, return status `MISSING_VCREDIST` with direct download link to `aka.ms/vs/17/release/vc_redist.x64.exe`.
  2. `checkOpenXrRuntime()`: Query `HKLM\SOFTWARE\Khronos\OpenXR\1` `ActiveRuntime`. If file does not exist on disk or registry is blank, return `OPENXR_UNAVAILABLE` with guidance for SteamVR / Meta Quest Link / Virtual Desktop.
  3. `checkNativeAssets()`: Scan `process.resourcesPath` (or `build/bin/` in dev) for all required assets (`vrinject.dll`, `vr-inject-cli.exe`, `onnxruntime.dll`, `DirectML.dll`, `shaders/`). Verify file sizes > 0.
  4. `checkOsBaseline()`: Check `os.release()` or `Win32_OperatingSystem.BuildNumber`. If `< 17763`, flag DirectML incompatibility.
* **IPC Channel:** Expose `doctor:diagnose` to the React renderer via `preload.ts`.

### Phase 2: UI Health Dashboard & One-Click Fixes (`HealthBadge.tsx` / `AboutPanel.tsx`)
**Goal:** Provide immediate visual feedback to the user on clean machines.

* **Components to Modify/Create:**
  - `nexvr-client/launcher/src/components/SystemHealthBanner.tsx`: Shows non-intrusive warning banners only when an environment issue is detected (e.g., *"Visual C++ Runtime Missing"* or *"OpenXR Inactive"*).
  - Add **"Check System Health"** and **"Export Diagnostic Bundle"** buttons inside [AboutPanel.tsx](file:///c:/Users/sathi/.gemini/antigravity/scratch/vr-inject/nexvr-client/launcher/src/components/AboutPanel.tsx).
  - Keep styling strictly adhering to `--ag-accent` (#FF4D4D) and dark surface tokens.

### Phase 3: Staging & Injection Hardening (`injectionManager.ts`)
**Goal:** Eliminate Win32 Error 5 (`ERROR_ACCESS_DENIED`) and `0xC0000135` (`STATUS_DLL_NOT_FOUND`).

* **Modifications in `injectionManager.ts`:**
  1. **Process Elevation Probe:** Before calling `vr-inject-cli.exe`, check if the target process has high integrity:
     ```powershell
     (Get-Process -Id $pid).StartInfo.Verb  # or CimInstance TokenElevation
     ```
     If the target process is elevated and the launcher is standard user, automatically invoke `Start-Process -FilePath vr-inject-cli.exe -Verb RunAs`.
  2. **Support DLL Staging Verification:** Confirm that `onnxruntime.dll` and `DirectML.dll` copied successfully into the target directory and match sizes. If copy fails due to folder permissions, elevate copy operation.

### Phase 4: Diagnostic Bundle Automation (`diagnosticsManager.ts`)
**Goal:** Empower users to generate complete, evidence-based triage reports with a single click.

* **Enhancements in `diagnosticsManager.ts`:**
  - Enhance `log:export` to compile a standardized `system_info.json`:
    - Application Version
    - OS Caption, Version, Build Number, Architecture
    - GPU Controller (Name, DriverVersion, AdapterRAM)
    - Active OpenXR Runtime path and existence
    - Installed VC++ Redistributable version
    - Injection history and last 50KB of sanitized logs
  - Zip everything into `Desktop/NexVR_Diagnostic_Report_<timestamp>.zip`.

### Phase 5: Automated Clean-Machine CI Verification (`scripts/verify_clean_machine.ps1`)
**Goal:** Test every production release candidate against clean-machine criteria before publishing.

* **File to Create:** `scripts/verify_clean_machine.ps1`
* **Automated Checks:**
  1. Inspect `nexvr-client/launcher/dist-electron/` production installer.
  2. Run dependency walker check on `vrinject.dll` and `vr-inject-cli.exe` to verify no accidental dependencies on dev-only DLLs (`msvcp140d.dll`, `ucrtbased.dll`, etc.).
  3. Validate that no hardcoded paths (`C:\Users\...`) exist inside any bundled JavaScript or binary string tables.
  4. Generate and launch a Windows Sandbox configuration (`test_sandbox.wsb`) for automated testing.

---

## 3. File Modification Plan

| File Path | Action | Description |
| :--- | :---: | :--- |
| `nexvr-client/launcher/electron/systemDoctor.ts` | **Create** | Implements registry checks (VC++, OpenXR), asset hashing, and OS baseline checks. |
| `nexvr-client/launcher/electron/diagnosticsManager.ts` | **Modify** | Enrich diagnostic bundle with hardware and registry metadata. |
| `nexvr-client/launcher/electron/preload.ts` | **Modify** | Expose `systemDoctor` APIs to renderer securely via `contextBridge`. |
| `nexvr-client/launcher/src/types.ts` | **Modify** | Add `SystemHealthReport` and `DoctorCheckResult` type definitions. |
| `nexvr-client/launcher/src/components/AboutPanel.tsx` | **Modify** | Add "System Health" & "Export Diagnostic Bundle" action buttons. |
| `scripts/verify_clean_machine.ps1` | **Create** | Automated pre-release verification script for clean Windows environments. |
| `docs/USER_PARITY_IMPLEMENTATION_PLAN.md` | **Create** | This detailed blueprint and tracking guide. |

---

## 4. Verification & Testing Strategy

```
                          VERIFICATION MATRIX
┌──────────────────────────────────────┬──────────────────────────────────────┐
│ Test Case                            │ Verification Method                  │
├──────────────────────────────────────┼──────────────────────────────────────┤
│ 1. Clean Machine (No VC++ installed) │ systemDoctor returns WARNING;        │
│                                      │ UI presents direct VC++ download CTA │
├──────────────────────────────────────┼──────────────────────────────────────┤
│ 2. Missing OpenXR Runtime            │ systemDoctor detects inactive key;   │
│                                      │ UI guides user to SteamVR/Oculus Link│
├──────────────────────────────────────┼──────────────────────────────────────┤
│ 3. Elevated Game (Steam Admin)       │ injectionManager auto-prompts UAC    │
│                                      │ without throwing Win32 Error 5       │
├──────────────────────────────────────┼──────────────────────────────────────┤
│ 4. Staged DLLs in Game Dir           │ Target game folder contains verified │
│                                      │ onnxruntime.dll and DirectML.dll     │
├──────────────────────────────────────┼──────────────────────────────────────┤
│ 5. One-Click Diagnostic Export       │ Clicking button generates valid .zip │
│                                      │ on Desktop with sanitized logs       │
└──────────────────────────────────────┴──────────────────────────────────────┘
```

1. **Unit & Static Testing:**
   - Run `npm run test:ci` in `nexvr-client/launcher`.
   - Run `scripts/check_coupling.ps1` to ensure singleton and architectural coupling budgets remain clean.
2. **End-to-End Clean Machine Verification:**
   - Execute `powershell -File scripts/verify_clean_machine.ps1`.
   - Test installer inside `WindowsSandbox.exe`.

---

## 5. Rollback & Safety Boundaries

- **Zero Breaking Changes to Injection:** All doctor checks are diagnostic and advisory. If registry queries fail or timeout, the launcher gracefully defaults to permitting injection attempts.
- **Fail-Closed Privacy:** Telemetry and diagnostic exports will continue to redact all user home paths and IP addresses before writing to disk or transmitting to `/api/report`.
