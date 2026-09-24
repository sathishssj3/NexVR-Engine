import { expect, test } from '@playwright/test';
import * as fs from 'fs';
import * as path from 'path';

const launcherRoot = path.resolve(__dirname, '..');
const repoRoot = path.resolve(launcherRoot, '..');

function readRepoFile(...parts: string[]) {
  return fs.readFileSync(path.join(repoRoot, ...parts), 'utf-8');
}

test.describe('Native asset packaging regression tests', () => {
  test('electron-builder packages every native asset the launcher deploys', () => {
    const builderConfig = readRepoFile('launcher', 'electron-builder.config.js');
    const injectionManager = readRepoFile('launcher', 'electron', 'injectionManager.ts');

    // Assets the injector deploys at runtime, resolved relative to
    // process.resourcesPath. Note: openxr_loader is statically linked into
    // vrinject.dll (see CMake target_link_libraries), so it is intentionally
    // NOT shipped as a standalone DLL and is absent from this list.
    const requiredResources = [
      'vrinject.dll',
      'vr-inject-cli.exe',
      'onnxruntime.dll',
      'DirectML.dll',
      'shaders',
      'models',
    ];

    for (const resource of requiredResources) {
      expect(builderConfig, `${resource} must be included in extraResources`).toContain(`to:   '${resource}'`);
    }

    // Runtime asset resolution lives in injectionManager, anchored to
    // process.resourcesPath (it moved out of main.ts during refactoring).
    expect(injectionManager).toContain('process.resourcesPath');
    expect(injectionManager).toContain(`resolveWithinRoot(canonicalBinSourceDir, 'vr-inject-cli.exe')`);
  });

  test('packaging paths match CMake bin output directory', () => {
    const cmakeCompilerFlags = readRepoFile('cmake', 'CompilerFlags.cmake');
    const builderConfig = readRepoFile('launcher', 'electron-builder.config.js');

    expect(cmakeCompilerFlags).toContain('set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)');
    expect(builderConfig).toContain("from: '../build/bin/vrinject.dll'");
    expect(builderConfig).toContain("from: '../build/bin/vr-inject-cli.exe'");
  });
});

test.describe('Native source safety regression tests', () => {
  test('DX12 hook checks dummy object creation before dereferencing vtables', () => {
    const dx12Hook = readRepoFile('src', 'hooks', 'dx12_hook.cpp');

    expect(dx12Hook).toMatch(/if\s*\(\s*FAILED\s*\([^)]*CreateCommandQueue/);
    expect(dx12Hook).toMatch(/if\s*\(\s*FAILED\s*\([^)]*CreateDXGIFactory1/);
    expect(dx12Hook).toMatch(/if\s*\(\s*FAILED\s*\([^)]*CreateSwapChain/);
    expect(dx12Hook).toMatch(/if\s*\(\s*!pSwapChain\s*\)/);
  });

  test('DX11 lifecycle manager validates the device from the swapchain before use', () => {
    // Device validation moved from dx11_hook.cpp into the lifecycle manager,
    // which now owns the DX11 device/swapchain state machine.
    const dx11Lifecycle = readRepoFile('src', 'rendering', 'dx11', 'dx11_lifecycle_manager.cpp');

    expect(dx11Lifecycle).toMatch(/GetDevice\(__uuidof\(ID3D11Device\)/);
    expect(dx11Lifecycle).toMatch(/FAILED\([^)]*GetDevice/);
    expect(dx11Lifecycle).toContain('Degrade(');
  });

  test('DX12 renderer creates a root signature before compute PSO creation', () => {
    const dx12Renderer = readRepoFile('src', 'rendering', 'backends', 'dx12_renderer.cpp');

    expect(dx12Renderer).toContain('CreateRootSignature');
    expect(dx12Renderer).toMatch(/psoDesc\.pRootSignature\s*=\s*[^;]+/);
  });

  test('neural inpainter is a null-safe, honestly-labeled passthrough placeholder', () => {
    // v1.0 fills disocclusion via disocclusion_fill.hlsl; the neural inpainter
    // is an explicit passthrough placeholder (real ONNX inference is tracked as
    // a follow-up). This guards two things so it can't silently rot: (1) it must
    // null-check its inputs before use, and (2) it must be honestly labeled as a
    // placeholder rather than masquerading as real inference.
    const neuralInpainter = readRepoFile('src', 'rendering', 'neural_inpainter.cpp');

    expect(neuralInpainter).toMatch(/if\s*\(\s*!context\s*\|\|\s*!warpedColorSRV\s*\)\s*return nullptr;/);
    expect(neuralInpainter, 'placeholder status must be documented in-source').toMatch(/placeholder|passthrough/i);
  });
});

test.describe('Cross-game isolation and profile safety regression tests', () => {
  test('engine detector avoids brittle hardcoded executable name overrides', () => {
    const engineDetector = readRepoFile('src', 'core', 'engine_detector.cpp');

    // QUAL-04: No hardcoded game-specific branches like exeStr.find("HogwartsLegacy")
    // or broad exeStr.find("-Win64-Shipping") that misclassify UE4 games as UE5.
    expect(engineDetector).not.toContain('HogwartsLegacy');
    expect(engineDetector).not.toContain('PenguinHotel');
    expect(engineDetector).not.toMatch(/exeStr\.find\("-Win64-Shipping"\)/);

    // Must support profile-first detection
    expect(engineDetector).toContain('profileDetection');
    expect(engineDetector).toContain('engineType');
  });

  test('configManager synchronizes vrinject.json across installPath and targetExeDir', () => {
    const configManager = readRepoFile('launcher', 'electron', 'configManager.ts');

    expect(configManager).toContain('dirsToSync');
    expect(configManager).toContain('candidatePaths');
    expect(configManager).toContain('targetDir');
  });

  test('injectionManager synchronizes vrinject.json and allows subfolder custom executables', () => {
    const injectionManager = readRepoFile('launcher', 'electron', 'injectionManager.ts');

    expect(injectionManager).toContain('rootConfigPath');
    expect(injectionManager).toContain('resolveWithinRoot(installPath, path.relative(installPath, customExe))');
  });

  test('curated profiles preserve engine invariants and matrix precision', () => {
    const configManager = readRepoFile('launcher', 'electron', 'configManager.ts');
    const injectionManager = readRepoFile('launcher', 'electron', 'injectionManager.ts');
    const cameraTracker = readRepoFile('src', 'memory_scanner', 'camera_delta_tracker.cpp');

    // configManager must preserve engine metadata and precision on write
    expect(configManager).toContain('validCfg.matrixPrecision ?? baseProfile.matrixPrecision');
    expect(configManager).toContain('validCfg.engine ?? baseProfile.engine');

    // injectionManager must enforce curated engine and matrix precision to target directories
    expect(injectionManager).toContain('baseProfile.matrixPrecision');
    expect(injectionManager).toContain('baseProfile.engine');

    // camera tracker must respect explicit matrixPrecision configuration over UE5 heuristics
    expect(cameraTracker).toContain('matrixPrecision == "Double64"');
    expect(cameraTracker).toContain('matrixPrecision == "Float32"');
  });

  test('frameCoordinator guards OpenXR runtime access on early frames', () => {
    const frameCoord = readRepoFile('src', 'core', 'frame_coordinator.cpp');

    expect(frameCoord).toMatch(/if\s*\(\s*m_oxrRuntime\s*\)\s*\{[\s\S]*m_oxrRuntime->GetState\(\)\s*==\s*openxr::RuntimeState::SYSTEM_SELECTED/);
    expect(frameCoord).toMatch(/bool\s+xrReady\s*=\s*m_oxrRuntime\s*&&/);
  });

  test('shader and renderer preserve per-game sRGB tonemapping isolation without cross-game regression', () => {
    const shader = readRepoFile('shaders', 'stereo_reprojection.hlsl');
    const dx12ManagerH = readRepoFile('src', 'rendering', 'dx12', 'dx12_stereo_resource_manager.h');
    const dx12ManagerCpp = readRepoFile('src', 'rendering', 'dx12', 'dx12_stereo_resource_manager.cpp');
    const stereoRendererH = readRepoFile('src', 'rendering', 'stereo', 'stereo_renderer.h');
    const stereoRendererCpp = readRepoFile('src', 'rendering', 'stereo', 'stereo_renderer.cpp');

    // Shader constant buffer must declare SrgbCorrection for ABI alignment
    expect(shader).toContain('uint SrgbCorrection;');
    expect(shader).toContain('float Contrast;');
    expect(shader).toContain('float Saturation;');
    expect(shader).toContain('float Brightness;');
    expect(shader).toContain('ApplyPerceptualGrading');

    // Reprojection shader must NOT perform artificial double-gamma pow 2.2 crush
    expect(shader).not.toContain('ApplyGammaCorrection');
    expect(shader).not.toContain('pow(clamp(color, 0.0f, 1.0f), 2.2f)');
    expect(shader).not.toContain('pow(max(outColor.rgb, 0.0f), 2.2f)');
    expect(shader).not.toContain('pow(max(leftColor.rgb, 0.0f), 2.2f)');

    // Launcher configuration manager and settings defaults must enforce universal srgbCorrection true
    const configMgr = readRepoFile('launcher', 'electron', 'configManager.ts');
    const settingsView = readRepoFile('launcher', 'src', 'components', 'SettingsView.tsx');
    expect(configMgr).toMatch(/defaultVRConfig:\s*VRConfig\s*=\s*\{[\s\S]*srgbCorrection:\s*true/);
    expect(settingsView).toContain('srgbCorrection: true');

    // C++ structs must declare contrast, saturation, brightness and srgbCorrection
    expect(dx12ManagerH).toContain('float contrast;');
    expect(dx12ManagerH).toContain('float saturation;');
    expect(dx12ManagerH).toContain('float brightness;');
    expect(dx12ManagerH).toContain('uint32_t srgbCorrection;');
    expect(dx12ManagerCpp).toContain('consts.contrast');
    expect(dx12ManagerCpp).toContain('consts.saturation');
    expect(dx12ManagerCpp).toContain('consts.brightness');
    expect(dx12ManagerCpp).toContain('consts.srgbCorrection');

    expect(stereoRendererH).toContain('float contrast;');
    expect(stereoRendererH).toContain('float saturation;');
    expect(stereoRendererH).toContain('float brightness;');
    expect(stereoRendererH).toContain('uint32_t srgbCorrection;');
    expect(stereoRendererCpp).toContain('constants->contrast');
    expect(stereoRendererCpp).toContain('constants->saturation');
    expect(stereoRendererCpp).toContain('constants->brightness');
    expect(stereoRendererCpp).toContain('constants->srgbCorrection');

    // Curated profiles: Sekiro and Hogwarts Legacy calibrated to desktop richness with universal sRGB linearization
    const sekiroProfile = JSON.parse(readRepoFile('profiles', '814380_sekiro.json'));
    const hogwartsProfile = JSON.parse(readRepoFile('profiles', '990080_hogwarts_legacy.json'));
    expect(sekiroProfile.srgbCorrection).toBe(true);
    expect(hogwartsProfile.srgbCorrection).toBe(true);
    expect(hogwartsProfile.contrast).toBe(1.20);
    expect(hogwartsProfile.saturation).toBe(1.15);
    expect(hogwartsProfile.brightness).toBe(1.14);
  });

  test('OTA manifest and injectionManager prevent stale cache shadowing and cross-game contamination', () => {
    const manifest = JSON.parse(readRepoFile('..', 'updates', 'manifest.json'));
    const updateMgr = readRepoFile('launcher', 'electron', 'updateManager.ts');
    const injectionMgr = readRepoFile('launcher', 'electron', 'injectionManager.ts');
    const workflow = readRepoFile('..', '.github', 'workflows', 'package-setup.yml');

    // Manifest must include shader assets for dynamic compilation
    expect(manifest.files).toContain('shaders/stereo_reprojection.hlsl');

    // updateManager must purge stale OTA cache when installed app is newer
    expect(updateMgr).toContain('purgeStaleOtaCacheIfAppNewer');
    expect(updateMgr).toContain('compareSemver');

    // updateManager must NEVER contain hardcoded legacy 0.1.0 fallbacks
    expect(updateMgr).not.toContain("'0.1.0'");
    expect(updateMgr).not.toContain('"0.1.0"');

    // injectionManager must enforce srgbCorrection and depthSubmission invariants
    expect(injectionMgr).toContain('baseProfile.srgbCorrection !== undefined');
    expect(injectionMgr).toContain('baseProfile.depthSubmission !== undefined');

    // injectionManager must NOT write to global stageCfg vrinject.json that causes cross-game contamination
    expect(injectionMgr).not.toContain("const stageCfg = path.join(updatesDir, 'vrinject.json')");

    // package-setup.yml must dynamically determine release tag
    expect(workflow).toContain('Determine Release Tag');
    expect(workflow).toContain('RELEASE_TAG');
  });

  test('library scanner strictly filters non-game launcher utilities and validates authentic game installs', () => {
    const utilsTs = readRepoFile('launcher', 'electron', 'utils.ts');
    const libraryManagerTs = readRepoFile('launcher', 'electron', 'libraryManager.ts');

    // isIgnoredSoftware must filter out launcher utilities and services
    expect(utilsTs).toContain("'launcher'");
    expect(utilsTs).toContain("'epic games launcher'");
    expect(utilsTs).toContain("'epic online services'");
    expect(utilsTs).toContain("'directxredist'");
    expect(utilsTs).toContain("'crashreportclient'");

    // libraryManager must not scan Program Files (x86)\Epic Games
    expect(libraryManagerTs).not.toContain("'C:\\\\Program Files (x86)\\\\Epic Games'");

    // libraryManager must enforce .egstore check to confirm authentic Epic game install
    expect(libraryManagerTs).toContain("fs.existsSync(path.join(gamePath, '.egstore'))");

    // libraryManager must not contain brittle hardcoded Mortal Shell overrides (QUAL-04)
    expect(libraryManagerTs).not.toContain("if (sub.name.toLowerCase() === 'mortalshell') displayName = 'Mortal Shell'");
  });

  test('v0.1.58 high-reliability architecture: SEH crash boundaries, AC tripwire, and sync tooling', () => {
    const dx11Hook = readRepoFile('src', 'hooks', 'dx11_hook.cpp');
    const dx12Hook = readRepoFile('src', 'hooks', 'dx12_hook.cpp');
    const vkHook = readRepoFile('src', 'hooks', 'vulkan_hook.cpp');
    const mainInjector = readRepoFile('src', 'injector', 'main.cpp');
    const configMgr = readRepoFile('launcher', 'electron', 'configManager.ts');
    const pkgJson = JSON.parse(readRepoFile('launcher', 'package.json'));

    // 1. SEH Crash boundaries around Present and Resize hooks
    expect(dx11Hook).toContain('__try {');
    expect(dx11Hook).toContain('__except (EXCEPTION_EXECUTE_HANDLER)');
    expect(dx12Hook).toContain('__try {');
    expect(dx12Hook).toContain('__except (EXCEPTION_EXECUTE_HANDLER)');
    expect(vkHook).toContain('__try {');
    expect(vkHook).toContain('__except (EXCEPTION_EXECUTE_HANDLER)');

    // 2. Anti-Cheat tripwire detects EAC, BattlEye, and Vanguard
    expect(mainInjector).toContain('strict_ac_blocklist');
    expect(mainInjector).toContain('"easyanticheat.exe"');
    expect(mainInjector).toContain('"beservice.exe"');
    expect(mainInjector).toContain('"vgc.exe"');

    // 3. Dynamic custom profiles support
    expect(configMgr).toContain("'custom_profiles'");
    expect(configMgr).toContain('config:reloadProfiles');

    // 4. Asset sync and binary signing tooling registered in package.json
    expect(pkgJson.scripts['sync:assets']).toBeDefined();
    expect(pkgJson.scripts['sign:binaries']).toBeDefined();
    expect(pkgJson.version).toBe('0.1.90');
  });

  test('v0.1.90 in-headset VR dashboard, aspect ratio FOV normalization, universal sRGB, and launcher synchronization', () => {
    const runtimeStateCpp = readRepoFile('src', 'core', 'runtime_state.cpp');
    const versionHeader = readRepoFile('src', 'core', 'version.h');
    const injectionMgrTs = readRepoFile('launcher', 'electron', 'injectionManager.ts');
    const diagnosticsMgrTs = readRepoFile('launcher', 'electron', 'diagnosticsManager.ts');
    const appTsx = readRepoFile('launcher', 'src', 'App.tsx');
    const sessionLogTsx = readRepoFile('launcher', 'src', 'components', 'SessionLog.tsx');
    const heroCommandTsx = readRepoFile('launcher', 'src', 'components', 'HeroCommandCenter.tsx');
    const settingsPanelTsx = readRepoFile('launcher', 'src', 'components', 'SettingsPanel.tsx');
    const sidebarTsx = readRepoFile('launcher', 'src', 'components', 'Sidebar.tsx');
    const sehShieldH = readRepoFile('src', 'core', 'seh_shield.h');
    const frameCoordCpp = readRepoFile('src', 'core', 'frame_coordinator.cpp');
    const cameraTrackerCpp = readRepoFile('src', 'memory_scanner', 'camera_delta_tracker.cpp');
    const inputHookCpp = readRepoFile('src', 'hooks', 'input_hook.cpp');

    // 1. No stale v0.1.16 version strings remain anywhere in the engine or launcher
    expect(runtimeStateCpp).not.toContain('v0.1.16');
    expect(runtimeStateCpp).toContain('NEXVR_ENGINE_VERSION');
    expect(versionHeader).toContain('#define NEXVR_ENGINE_VERSION "0.1.90"');
    expect(injectionMgrTs).not.toContain('v0.1.16');
    expect(diagnosticsMgrTs).not.toContain('v0.1.16');

    // 2. Real-time log streaming stays active throughout running session
    expect(appTsx).toContain('await window.ag.inject.monitor(res.pid);');
    expect(appTsx).toContain('window.ag.log.offLine();');
    // Ensure offline is NOT called immediately between deploy and monitor
    const deployIndex = appTsx.indexOf('await window.ag.inject.deploy');
    const monitorIndex = appTsx.indexOf('await window.ag.inject.monitor');
    const offlineBetween = appTsx.slice(deployIndex, monitorIndex).includes('window.ag.log.offLine();\n    if (res.success');
    expect(offlineBetween).toBe(false);

    // 3. SessionLog renders lines directly without simulated typing lag and highlights [OK] green
    expect(sessionLogTsx).not.toContain('TypewriterLine');
    expect(sessionLogTsx).toContain('wordBreak: \'break-all\'');
    expect(sessionLogTsx).toContain('var(--ag-accent-success)');
    expect(sessionLogTsx).toContain('var(--ag-text-primary)');

    // 4. SEH Shield uses LOG_DEBUG for probe access violations to prevent 5000+ IOPS disk locks
    expect(sehShieldH).toContain('LOG_DEBUG("SEH Shield: Access Violation (TOCTOU)');
    expect(sehShieldH).not.toContain('LOG_WARN("SEH Shield: Access Violation (TOCTOU)');

    // 5. FrameCoordinator demotes per-frame stereo pipeline trace to LOG_DEBUG
    expect(frameCoordCpp).toContain('LOG_DEBUG("FrameCoordinator: Entering stereo pipeline');
    expect(frameCoordCpp).not.toContain('LOG_INFO("FrameCoordinator: Entering stereo pipeline');

    // 6. Tester-friendly milestone badges in camera tracker and input hook
    expect(cameraTrackerCpp).toContain('[OK] Camera Tracking: 6DOF View Matrix Locked');
    expect(inputHookCpp).toContain('[OK] Input System: VR Controllers & Gamepad Hooked');

    // 7. App.tsx batches IPC log updates via requestAnimationFrame to protect UI thread
    expect(appTsx).toContain('requestAnimationFrame');
    expect(appTsx).toContain('logQueueRef');

    // 8. Minimalist full-window showcase with smoke gray borders and Sidebar GAME LIBRARY header
    expect(sidebarTsx).toContain('GAME LIBRARY');
    expect(sidebarTsx).toContain('+ ADD GAME');
    expect(sidebarTsx).toContain('SEARCH TITLES...');
    expect(heroCommandTsx).toContain('settings-card');
    expect(settingsPanelTsx).toContain('settings-card');
    expect(settingsPanelTsx).toContain('PER-TITLE VR CONFIGURATION');
  });
});

