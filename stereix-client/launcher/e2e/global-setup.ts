/**
 * Playwright Global Setup
 * Ensures electron-dist/ is built before E2E tests that launch Electron.
 * Runs `npm run build` (vite build + tsc) so electron-dist/electron/main.js exists.
 */
import { execSync } from 'child_process';
import * as path from 'path';
import * as fs from 'fs';

export default async function globalSetup() {
  const launcherRoot = path.resolve(__dirname, '..');
  const mainJs = path.join(launcherRoot, 'electron-dist', 'electron', 'main.js');

  // Only rebuild if electron-dist/electron/main.js is missing or stale
  const srcMain = path.join(launcherRoot, 'electron', 'main.ts');
  const needsBuild =
    !fs.existsSync(mainJs) ||
    fs.statSync(srcMain).mtimeMs > fs.statSync(mainJs).mtimeMs;

  if (needsBuild) {
    console.log('[global-setup] Building electron-dist/ for E2E tests...');
    execSync('npm run build', {
      cwd: launcherRoot,
      stdio: 'inherit',
      timeout: 60000,
    });
    console.log('[global-setup] Build complete.');
  } else {
    console.log('[global-setup] electron-dist/ is up to date, skipping build.');
  }
}
