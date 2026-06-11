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
    const electronMain = readRepoFile('launcher', 'electron', 'main.ts');

    const requiredResources = [
      'vrinject.dll',
      'vr-inject-cli.exe',
      'onnxruntime.dll',
      'openxr_loader.dll',
      'shaders',
      'models',
    ];

    for (const resource of requiredResources) {
      expect(electronMain).toContain(`process.resourcesPath, '${resource}'`);
      expect(builderConfig, `${resource} must be included in extraResources`).toContain(`to:   '${resource}'`);
    }
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

  test('DX11 hook validates the swapchain device before using it', () => {
    const dx11Hook = readRepoFile('src', 'hooks', 'dx11_hook.cpp');

    expect(dx11Hook).toMatch(/HRESULT\s+\w+\s*=\s*pSwapChain->GetDevice\(__uuidof\(ID3D11Device\)/);
    expect(dx11Hook).toMatch(/if\s*\(\s*FAILED\(\w+\)\s*\|\|\s*!g_frameResources\.device\s*\)/);
  });

  test('DX12 renderer creates a root signature before compute PSO creation', () => {
    const dx12Renderer = readRepoFile('src', 'rendering', 'backends', 'dx12_renderer.cpp');

    expect(dx12Renderer).toContain('CreateRootSignature');
    expect(dx12Renderer).toMatch(/psoDesc\.pRootSignature\s*=\s*[^;]+/);
  });

  test('neural inpainter uses real ONNX Runtime inference instead of the prototype mock', () => {
    const neuralInpainter = readRepoFile('src', 'rendering', 'neural_inpainter.cpp');

    expect(neuralInpainter).not.toContain('mocking the ORT calls');
    expect(neuralInpainter).toContain('#include <onnxruntime_cxx_api.h>');
    expect(neuralInpainter).toMatch(/m_session->Run\s*\(/);
  });
});
