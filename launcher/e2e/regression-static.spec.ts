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
    const cmake = readRepoFile('CMakeLists.txt');
    const builderConfig = readRepoFile('launcher', 'electron-builder.config.js');

    expect(cmake).toContain('set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)');
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
    const dx11Lifecycle = readRepoFile('src', 'core', 'dx11_lifecycle_manager.cpp');

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
