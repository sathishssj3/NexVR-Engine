import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';
import { execSync } from 'child_process';
import asar from '@electron/asar';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const launcherDir = path.resolve(__dirname, '..');

const installedAppDir = path.join(
  process.env.LOCALAPPDATA || path.join(os.homedir(), 'AppData', 'Local'),
  'Programs',
  'launcher'
);
const installedResourcesDir = path.join(installedAppDir, 'resources');
const installedAsarPath = path.join(installedResourcesDir, 'app.asar');

console.log(`[PatchLauncher] Target installed app: ${installedAppDir}`);
console.log(`[PatchLauncher] Target app.asar: ${installedAsarPath}`);

if (!fs.existsSync(installedAsarPath)) {
  console.error(`[PatchLauncher] ERROR: Installed app.asar not found at ${installedAsarPath}`);
  process.exit(1);
}

// 1. Kill any running NexVR Engine instance so file handles are unlocked
try {
  if (process.platform === 'win32') {
    execSync('taskkill /F /IM "NexVR Engine.exe" /T 2>nul', { stdio: 'ignore' });
    execSync('taskkill /F /IM electron.exe /T 2>nul', { stdio: 'ignore' });
    console.log('[PatchLauncher] Terminated running engine/electron processes to unlock files.');
  }
} catch {}

// Wait 500ms for OS to release file lock
await new Promise(r => setTimeout(r, 500));

// 2. Extract installed app.asar to temporary directory
const tempExtractDir = path.join(os.tmpdir(), `nexvr_asar_patch_${Date.now()}`);
fs.mkdirSync(tempExtractDir, { recursive: true });
console.log(`[PatchLauncher] Extracting existing app.asar to ${tempExtractDir}...`);
asar.extractAll(installedAsarPath, tempExtractDir);

// 3. Overwrite frontend-dist with the latest build
const srcFrontendDist = path.join(launcherDir, 'frontend-dist');
const dstFrontendDist = path.join(tempExtractDir, 'frontend-dist');
console.log(`[PatchLauncher] Copying latest frontend-dist from ${srcFrontendDist}...`);
fs.cpSync(srcFrontendDist, dstFrontendDist, { recursive: true, force: true });

// 4. Overwrite electron-dist with the latest build
const srcElectronDist = path.join(launcherDir, 'electron-dist');
const dstElectronDist = path.join(tempExtractDir, 'electron-dist');
console.log(`[PatchLauncher] Copying latest electron-dist from ${srcElectronDist}...`);
fs.cpSync(srcElectronDist, dstElectronDist, { recursive: true, force: true });

// 5. Overwrite package.json
const srcPkg = path.join(launcherDir, 'package.json');
const dstPkg = path.join(tempExtractDir, 'package.json');
fs.copyFileSync(srcPkg, dstPkg);

// 6. Repack into installed app.asar (with backup)
const backupAsarPath = path.join(installedResourcesDir, `app.asar.bak_${Date.now()}`);
console.log(`[PatchLauncher] Backing up previous asar to ${backupAsarPath}...`);
fs.copyFileSync(installedAsarPath, backupAsarPath);

console.log(`[PatchLauncher] Repacking new asar to ${installedAsarPath}...`);
await asar.createPackage(tempExtractDir, installedAsarPath);

// 7. Synchronize native engine binaries & assets to installed resources
const rootDir = path.resolve(launcherDir, '..', '..');
const updatesDir = path.join(
  process.env.APPDATA || path.join(os.homedir(), 'AppData', 'Roaming'),
  'NexVR Engine',
  'updates'
);

const binariesToSync = ['vrinject.dll', 'vr-inject-cli.exe'];
for (const bin of binariesToSync) {
  let srcBin = path.join(updatesDir, bin);
  if (!fs.existsSync(srcBin)) {
    srcBin = path.join(rootDir, 'build', 'bin', bin);
  }
  if (fs.existsSync(srcBin)) {
    const dstBin = path.join(installedResourcesDir, bin);
    try {
      fs.copyFileSync(srcBin, dstBin);
      console.log(`[PatchLauncher] Synced native binary ${bin} -> ${dstBin}`);
    } catch (e) {
      console.warn(`[PatchLauncher] Could not copy ${bin}:`, e.message);
    }
  }
}

// 8. Clean up temp folder
try {
  fs.rmSync(tempExtractDir, { recursive: true, force: true });
} catch {}

// 9. Clean up old backups, keeping only the last 2
try {
  const allBackups = fs.readdirSync(installedResourcesDir)
    .filter(f => f.startsWith('app.asar.bak_'))
    .sort()
    .reverse();
  for (const oldBackup of allBackups.slice(2)) {
    fs.unlinkSync(path.join(installedResourcesDir, oldBackup));
  }
} catch {}

console.log('[PatchLauncher] Successfully patched installed NexVR Engine launcher!');
