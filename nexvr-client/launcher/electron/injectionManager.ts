import { app, shell, ipcMain } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import * as util from 'util';
import { InjectResult } from '../src/types';
import {
  assertTrustedIpcSender,
  canonicalExistingPath,
  gamePathsMap,
  gameExeMap,
  resolveWithinRoot,
  safeGamePath,
  validateGameId,
} from './utils';
import { detectAntiCheat } from './libraryManager';
import { getLocalManifest, compareSemver } from './updateManager';
import { sendDiscordTelemetry } from './telemetryManager';

const execFileAsync = util.promisify(child_process.execFile);
const isDev = !app.isPackaged;

export let cancelInjectionFlag = false;
export let activeTargetExeName = '';
export let activeTargetPid = 0;
export let activeGameId = '';
export let activeSessionStartTime = 0;
export let activeSessionConfig: any = null;

const injectRateLimits: Record<string, number[]> = {};
let injectionInProgress = false;
let activeLogPath = '';

export function pickPreferredAsset(canonicalPath: string, otaPath: string, minSize = 0): string {
  const otaExists = fs.existsSync(otaPath) && (minSize === 0 || fs.statSync(otaPath).size >= minSize);
  const canonExists = fs.existsSync(canonicalPath) && (minSize === 0 || fs.statSync(canonicalPath).size >= minSize);

  if (!otaExists) return canonicalPath;
  if (!canonExists) return otaPath;

  // In development mode (npm run dev / unpacked), local build artifacts must ALWAYS take precedence over downloaded OTA cache!
  if (!app.isPackaged) {
    return canonicalPath;
  }

  // In packaged mode, if the app version matches or exceeds the OTA manifest version,
  // the bundled binaries are up-to-date and MUST take precedence over stale cached files!
  try {
    const local = getLocalManifest();
    const appVer = app.getVersion();
    if (local && appVer && compareSemver(appVer, local.engineVersion) >= 0) {
      return canonicalPath;
    }
  } catch {}

  // If OTA is strictly newer than the bundled binary by timestamp, use OTA hotfix
  try {
    const otaMtime = fs.statSync(otaPath).mtimeMs;
    const canonMtime = fs.statSync(canonicalPath).mtimeMs;
    return otaMtime >= canonMtime ? otaPath : canonicalPath;
  } catch {
    return canonicalPath;
  }
}

async function terminatePid(pid: number, force = false): Promise<void> {
  if (!Number.isSafeInteger(pid) || pid <= 0) return;
  const args = ['/PID', String(pid)];
  if (force) args.push('/F');
  await execFileAsync('taskkill.exe', args, {
    timeout: 3000,
    windowsHide: true,
  }).catch(() => {});
}

async function getProcessPath(pid: number): Promise<string> {
  if (!Number.isSafeInteger(pid) || pid <= 0) return '';
  try {
    const command = `try { (Get-Process -Id ${pid} -ErrorAction Stop).Path } catch { (Get-CimInstance Win32_Process -Filter "ProcessId = ${pid}" -ErrorAction SilentlyContinue).ExecutablePath }`;
    const { stdout } = await execFileAsync(
      'powershell.exe',
      ['-NoProfile', '-NonInteractive', '-Command', command],
      { encoding: 'utf-8', timeout: 3000, windowsHide: true }
    );
    return stdout.trim();
  } catch {
    return '';
  }
}

let activeWatchedLogPaths: string[] = [];
let activeWatchers: fs.FSWatcher[] = [];
let watchedSizes: Record<string, number> = {};
let watchedRemainders: Record<string, string> = {};
export let currentSessionLogPath = '';
export let latestSessionLogPath = '';

export function getLogsDir(): string {
  const logsDir = path.join(app.getPath('userData'), 'logs');
  if (!fs.existsSync(logsDir)) {
    fs.mkdirSync(logsDir, { recursive: true });
  }
  return logsDir;
}

export function getLatestSessionLogPath(): string {
  return latestSessionLogPath || path.join(getLogsDir(), 'latest_session.log');
}

export function emitLogLine(event: Electron.IpcMainInvokeEvent, line: string): void {
  const trimmed = line.trim();
  if (!trimmed) return;

  try {
    if (!event.sender.isDestroyed()) {
      event.sender.send('log:line', trimmed.slice(0, 2000));
    }
  } catch {}

  const chunk = trimmed + '\n';
  try {
    if (currentSessionLogPath) {
      fs.appendFile(currentSessionLogPath, chunk, 'utf-8', () => {});
    }
    if (latestSessionLogPath) {
      fs.appendFile(latestSessionLogPath, chunk, 'utf-8', () => {});
    }
  } catch {}
}

let isReadingLogs = false;
function readNewBytes(normalized: string, event: Electron.IpcMainInvokeEvent): void {
  if (!fs.existsSync(normalized) || isReadingLogs) return;
  isReadingLogs = true;

  try {
    const fd = fs.openSync(normalized, 'r');
    try {
      const stat = fs.fstatSync(fd);
      let lastSize = watchedSizes[normalized] ?? 0;
      if (stat.size < lastSize) {
        lastSize = 0;
        watchedSizes[normalized] = 0;
        watchedRemainders[normalized] = '';
      }
      if (stat.size <= lastSize) return;

      const length = Math.min(stat.size - lastSize, 2 * 1024 * 1024);
      const buf = Buffer.alloc(length);
      const bytesRead = fs.readSync(fd, buf, 0, length, lastSize);
      watchedSizes[normalized] = lastSize + bytesRead;

      const text = (watchedRemainders[normalized] || '') + buf.subarray(0, bytesRead).toString('utf-8');
      const lines = text.split(/\r?\n/);
      watchedRemainders[normalized] = lines.pop() || '';

      const validLines: string[] = [];
      for (const line of lines) {
        const trimmed = line.trim();
        if (trimmed) validLines.push(trimmed);
      }

      if (validLines.length > 0) {
        // Non-blocking asynchronous batch append to session logs
        const chunk = validLines.join('\n') + '\n';
        if (currentSessionLogPath) {
          fs.appendFile(currentSessionLogPath, chunk, 'utf-8', () => {});
        }
        if (latestSessionLogPath) {
          fs.appendFile(latestSessionLogPath, chunk, 'utf-8', () => {});
        }

        // Bounded IPC dispatch to prevent event loop starvation
        if (!event.sender.isDestroyed()) {
          const toSend = validLines.length > 60 ? validLines.slice(validLines.length - 60) : validLines;
          for (const l of toSend) {
            event.sender.send('log:line', l.slice(0, 2000));
          }
        }
      }
    } finally {
      fs.closeSync(fd);
    }
  } catch {} finally {
    isReadingLogs = false;
  }
}

function stopActiveLogWatch(): void {
  for (const p of activeWatchedLogPaths) {
    try {
      fs.unwatchFile(p);
    } catch {}
  }
  for (const w of activeWatchers) {
    try {
      w.close();
    } catch {}
  }
  activeWatchers = [];
  activeWatchedLogPaths = [];
  watchedSizes = {};
  watchedRemainders = {};
  activeLogPath = '';
  isReadingLogs = false;
}

function startWatchingLogFile(filePath: string, event: Electron.IpcMainInvokeEvent): void {
  const normalized = path.resolve(filePath);
  if (activeWatchedLogPaths.includes(normalized)) return;
  activeWatchedLogPaths.push(normalized);

  let initialSize = 0;
  try {
    if (fs.existsSync(normalized)) {
      initialSize = fs.statSync(normalized).size;
    }
  } catch {}
  watchedSizes[normalized] = initialSize;
  watchedRemainders[normalized] = '';

  // Clean, focused 150ms handle polling without directory storm noise
  fs.watchFile(normalized, { interval: 150 }, () => {
    readNewBytes(normalized, event);
  });

  // Flush any content already written
  readNewBytes(normalized, event);
}

ipcMain.handle('inject:cancel', async (event) => {
  assertTrustedIpcSender(event);
  cancelInjectionFlag = true;

  const pid = activeTargetPid;
  if (pid > 0) {
    await terminatePid(pid);
    setTimeout(() => void terminatePid(pid, true), 2000);
  }

  sendDiscordTelemetry({
    gameId: activeGameId || 'cancelled_session',
    gameName: activeTargetExeName,
    status: 'error',
    message: 'User cancelled injection or session was terminated.',
    logFilePath: currentSessionLogPath || latestSessionLogPath,
  }).catch(() => {});
});

ipcMain.handle('inject:deploy', async (event, id: string): Promise<InjectResult> => {
  try {
    assertTrustedIpcSender(event);
    const validId = validateGameId(id);

    if (injectionInProgress) {
      return { success: false, message: 'Another injection is already in progress.' };
    }
    injectionInProgress = true;

    const now = Date.now();
    const windowMs = 15 * 60 * 1000;
    injectRateLimits[validId] = (injectRateLimits[validId] || []).filter(t => now - t < windowMs);
    if (injectRateLimits[validId].length >= 5) {
      return { success: false, message: 'Rate limit exceeded.' };
    }
    injectRateLimits[validId].push(now);

    const registeredInstallPath = gamePathsMap[validId];
    if (!registeredInstallPath) return { success: false, message: 'Game path not found' };
    const installPath = canonicalExistingPath(registeredInstallPath, 'directory');

    // Anti-Cheat Guard: Block injection into protected games to prevent multiplayer account bans
    const ac = detectAntiCheat(installPath);
    if (ac.hasAntiCheat) {
      return {
        success: false,
        message: `Injection Blocked: ${ac.antiCheatName} detected. To protect your account from multiplayer bans, VR injection is disabled on this title.`
      };
    }

    const logPath = safeGamePath(installPath, 'vrinject.log');
    activeLogPath = logPath;

    stopActiveLogWatch();

    const logsDir = getLogsDir();
    const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
    currentSessionLogPath = path.join(logsDir, `session_${validId}_${timestamp}.log`);
    latestSessionLogPath = path.join(logsDir, 'latest_session.log');

    try {
      const appVer = app.getVersion() || '0.1.60';
      const header = `=== NexVR Engine Session Log [v${appVer}] ===\nGame ID: ${validId}\nStarted: ${new Date().toISOString()}\n==========================================\n`;
      fs.writeFileSync(currentSessionLogPath, header, 'utf-8');
      fs.writeFileSync(latestSessionLogPath, header, 'utf-8');
    } catch {}

    try {
      fs.writeFileSync(logPath, '');
    } catch {}

    emitLogLine(event, `[--] Initializing session diagnostics for game ID: ${validId}`);
    emitLogLine(event, `[--] Session log file: ${path.basename(currentSessionLogPath)}`);

    startWatchingLogFile(logPath, event);

    const localAppData = process.env.LOCALAPPDATA || '';
    if (localAppData) {
      const appDataLog = path.join(localAppData, 'VRInject', 'vrinject.log');
      try {
        const vrDir = path.dirname(appDataLog);
        if (!fs.existsSync(vrDir)) fs.mkdirSync(vrDir, { recursive: true });
        if (!fs.existsSync(appDataLog)) fs.writeFileSync(appDataLog, '', 'utf-8');
      } catch {}
      startWatchingLogFile(appDataLog, event);
    }

    const candidateBinDirs = [
      path.resolve(__dirname, '../../../../build/bin'),
      path.resolve(__dirname, '../../../build/bin'),
      path.resolve(__dirname, '../../build/bin'),
      path.resolve(app.getAppPath(), '../../build/bin'),
      path.resolve(app.getAppPath(), '../../../build/bin'),
      path.resolve(process.cwd(), '../build/bin'),
      path.resolve(process.cwd(), '../../build/bin'),
    ];
    const devBinDir = candidateBinDirs.find(
      (d) => fs.existsSync(d) && (fs.existsSync(path.join(d, 'vrinject.dll')) || fs.existsSync(path.join(d, 'vr-inject-cli.exe')))
    );
    const binSourceDir = (isDev || (!fs.existsSync(path.join(process.resourcesPath, 'vrinject.dll')) && devBinDir))
      ? (devBinDir || process.resourcesPath)
      : process.resourcesPath;
    const canonicalBinSourceDir = canonicalExistingPath(binSourceDir, 'directory');
    const otaCli = path.join(app.getPath('userData'), 'updates', 'vr-inject-cli.exe');
    const bundledCli = resolveWithinRoot(canonicalBinSourceDir, 'vr-inject-cli.exe');
    const canonCli = fs.existsSync(bundledCli)
      ? canonicalExistingPath(bundledCli, 'file')
      : bundledCli;
    const cliSource = pickPreferredAsset(canonCli, otaCli, 10000);
    const candidateShaderDirs = [
      resolveWithinRoot(canonicalBinSourceDir, 'shaders'),
      path.resolve(__dirname, '../../../../build/bin/shaders'),
      path.resolve(__dirname, '../../../build/bin/shaders'),
      path.resolve(__dirname, '../../shaders'),
      path.resolve(process.resourcesPath, 'shaders'),
    ];
    const shadersSource = candidateShaderDirs.find((d) => fs.existsSync(d)) || resolveWithinRoot(canonicalBinSourceDir, 'shaders');
    const candidateModelDirs = [
      resolveWithinRoot(canonicalBinSourceDir, 'models'),
      path.resolve(__dirname, '../../../../nexvr-client/models'),
      path.resolve(__dirname, '../../../../models'),
      path.resolve(__dirname, '../../../models'),
      path.resolve(__dirname, '../../models'),
      path.resolve(process.resourcesPath, 'models'),
    ];
    const modelsSource = candidateModelDirs.find((d) => fs.existsSync(d)) || resolveWithinRoot(canonicalBinSourceDir, 'models');

    if (validId.startsWith('custom_') && gameExeMap[validId]) {
      const customExe = canonicalExistingPath(gameExeMap[validId], 'file');
      resolveWithinRoot(installPath, path.relative(installPath, customExe));
      // Use child_process.exec with Windows 'start' command to properly set CWD and handle ShellExecute
      // This bypasses Node's spawn EACCES limitation when launching games that require elevation or special permissions.
      const exeCwd = path.dirname(customExe);
      child_process.exec(`start "" /d "${exeCwd}" "${customExe}"`);
    } else if (/^\d+$/.test(validId)) {
      await shell.openExternal(`steam://rungameid/${validId}`);
    } else {
      await shell.openExternal(
        `com.epicgames.launcher://apps/${encodeURIComponent(validId)}?action=launch&silent=true`
      );
    }

    let targetExeName = '';
    let targetExeDir = installPath;
    const registeredExe = gameExeMap[validId];
    if (registeredExe) {
      const customExe = canonicalExistingPath(registeredExe, 'file');
      targetExeName = path.basename(customExe).toLowerCase();
      targetExeDir = path.dirname(customExe);
    } else {
      let largestSize = 0;
      let visitedEntries = 0;

      const scan = (dir: string, depth = 0) => {
        if (depth > 8 || visitedEntries >= 20000) return;
        try {
          const entries = fs.readdirSync(dir, { withFileTypes: true });
          for (const entry of entries) {
            if (++visitedEntries >= 20000) break;
            if (entry.isSymbolicLink()) continue;

            const fullPath = resolveWithinRoot(
              installPath,
              path.relative(installPath, path.join(dir, entry.name))
            );
            try {
              if (entry.isDirectory()) {
                scan(fullPath, depth + 1);
              } else if (entry.isFile() && entry.name.toLowerCase().endsWith('.exe')) {
                const lower = entry.name.toLowerCase();
                if (
                  lower.includes('launcher') ||
                  lower.includes('crashreporter') ||
                  lower.includes('crashhandler') ||
                  lower.includes('reporter') ||
                  lower.includes('anticheat') ||
                  lower.includes('eosbootstrapper') ||
                  lower.includes('start_protected_game')
                ) {
                  continue;
                }
                const size = fs.statSync(fullPath).size;
                if (size > largestSize) {
                  largestSize = size;
                  targetExeName = lower;
                  targetExeDir = dir;
                }
              }
            } catch {
              // Ignore inaccessible game subdirectories.
            }
          }
        } catch {
          // Ignore inaccessible game subdirectories.
        }
      };
      scan(installPath);
    }

    if (!targetExeName) {
      stopActiveLogWatch();
      return { success: false, message: 'Could not determine main executable name' };
    }

    try {
      const updatesDir = path.join(app.getPath('userData'), 'updates');
      const hotfixShaders = path.join(updatesDir, 'shaders');
      const activeShadersSource = (!app.isPackaged && fs.existsSync(shadersSource))
        ? shadersSource
        : (fs.existsSync(hotfixShaders) && fs.readdirSync(hotfixShaders).length > 0 ? hotfixShaders : shadersSource);

      const targetDirs = new Set<string>([targetExeDir, installPath]);
      for (const sub of ['Phoenix/Binaries/Win64', 'Chameleon/Binaries/Win64', 'Dungeonhaven/Binaries/Win64', 'Binaries/Win64']) {
        const subDir = path.join(installPath, sub);
        if (fs.existsSync(subDir)) targetDirs.add(subDir);
      }

      for (const d of targetDirs) {
        if (fs.existsSync(activeShadersSource)) {
          try {
            fs.cpSync(activeShadersSource, path.join(d, 'shaders'), {
              recursive: true,
              force: true,
              dereference: false,
            });
          } catch (e) {}
        }
        if (fs.existsSync(modelsSource)) {
          try {
            fs.cpSync(modelsSource, path.join(d, 'models'), {
              recursive: true,
              force: true,
              dereference: false,
            });
          } catch (e) {}
        }
      }

      // Copy ONNX and DirectML DLLs to prevent target process loader lock/freeze due to missing imports
      // Also copy vrinject.dll and openxr_loader.dll so the implicit Vulkan layer can pick it up BEFORE the game starts
      // Prioritize local builds in dev mode, or newer OTA hotfixed binaries if available in packaged mode!
      const dllsToCopy = ['onnxruntime.dll', 'DirectML.dll', 'vrinject.dll', 'openxr_loader.dll'];
      for (const dll of dllsToCopy) {
        const hotfixPath = path.join(updatesDir, dll);
        const canonPath = resolveWithinRoot(canonicalBinSourceDir, dll);
        const srcPath = pickPreferredAsset(canonPath, hotfixPath);

        if (fs.existsSync(srcPath)) {
          for (const d of targetDirs) {
            try {
              fs.copyFileSync(srcPath, path.join(d, dll));
              console.info(`Copied ${dll} to ${d} (source: ${srcPath === hotfixPath ? 'HOTFIX' : 'LOCAL/BUNDLED'})`);
            } catch (e) {
              console.warn(`Could not copy ${dll} to ${d} (already present or locked): ${e}`);
            }
          }
        }
      }

      // Ensure vrinject.json configuration is synchronized across all target binary directories
      const rootConfigPath = path.join(installPath, 'vrinject.json');
      let baseProfile: Record<string, any> = {};

      const profileDirs = [
        path.join(app.getPath('userData'), 'updates', 'profiles'),
        path.resolve(__dirname, '../../../../nexvr-client/profiles'),
        path.resolve(__dirname, '../../../../profiles'),
        path.resolve(__dirname, '../../../profiles'),
        path.resolve(__dirname, '../../profiles'),
        path.join(process.resourcesPath, 'profiles'),
      ];
      const exeBase = targetExeName ? path.basename(targetExeName, '.exe').toLowerCase() : '';
      for (const pDir of profileDirs) {
        try {
          if (fs.existsSync(pDir)) {
            for (const f of fs.readdirSync(pDir)) {
              const lowerF = f.toLowerCase();
              let isMatch = f.startsWith(`${validId}_`) || f === `${validId}.json`;
              if (!isMatch) {
                if (exeBase && (lowerF.includes(exeBase) || lowerF === `${exeBase}.json`)) isMatch = true;
                if (lowerF.includes('sekiro') && (exeBase.includes('sekiro') || validId.includes('sekiro') || installPath.toLowerCase().includes('sekiro'))) isMatch = true;
                if (lowerF.includes('mortal_shell') && (exeBase.includes('dungeonhaven') || exeBase.includes('mortalshell') || validId.includes('mortalshell') || installPath.toLowerCase().includes('mortalshell'))) isMatch = true;
                if (lowerF.includes('hogwarts') && (exeBase.includes('hogwarts') || exeBase.includes('phoenix') || validId.includes('hogwarts') || installPath.toLowerCase().includes('hogwarts'))) isMatch = true;
              }
              if (isMatch) {
                const parsed = JSON.parse(fs.readFileSync(path.join(pDir, f), 'utf-8'));
                baseProfile = { ...baseProfile, ...parsed };
                break;
              }
            }
          }
        } catch {}
      }

      // Hardened fallback for Sekiro if profile was not matched from disk
      if (Object.keys(baseProfile).length === 0 && (exeBase.includes('sekiro') || validId.includes('sekiro') || installPath.toLowerCase().includes('sekiro'))) {
        baseProfile = {
          id: '814380',
          name: 'Sekiro: Shadows Die Twice',
          engine: 'Generic',
          api: 'DX11',
          reverseZ: false,
          rowMajorMatrices: false,
          matrixPrecision: 'Float32',
          motionAimSensitivity: 1.0,
          useRecommendedResolution: true,
          srgbCorrection: false,
          depthSubmission: false,
          rawInputMode: true,
          autoInjectOnLaunch: true,
        };
      }

      // Hardened fallback for Mortal Shell if profile was not matched from disk
      if (Object.keys(baseProfile).length === 0 && (exeBase.includes('dungeonhaven') || exeBase.includes('mortalshell') || validId.includes('mortalshell') || validId.includes('1110910') || installPath.toLowerCase().includes('mortalshell'))) {
        baseProfile = {
          id: '1110910',
          name: 'Mortal Shell',
          engine: 'UnrealEngine4',
          api: 'DX11',
          reverseZ: true,
          rowMajorMatrices: true,
          matrixPrecision: 'Float32',
          motionAimSensitivity: 1.0,
          useRecommendedResolution: true,
          srgbCorrection: false,
          depthSubmission: false,
          rawInputMode: true,
          autoInjectOnLaunch: true,
          contrast: 1.18,
          saturation: 1.12,
          brightness: 1.15,
        };
      }

      // Hardened fallback for Hogwarts Legacy if profile was not matched from disk
      if (Object.keys(baseProfile).length === 0 && (exeBase.includes('hogwarts') || exeBase.includes('phoenix') || validId.includes('990080') || installPath.toLowerCase().includes('hogwarts'))) {
        baseProfile = {
          id: '990080',
          name: 'Hogwarts Legacy',
          engine: 'UnrealEngine4',
          api: 'DX12',
          reverseZ: true,
          rowMajorMatrices: true,
          matrixPrecision: 'Float32',
          motionAimSensitivity: 1.0,
          useRecommendedResolution: true,
          srgbCorrection: false,
          depthSubmission: true,
          rawInputMode: true,
          autoInjectOnLaunch: true,
          contrast: 1.20,
          saturation: 1.15,
          brightness: 1.14,
        };
      }

      let activeConfig: Record<string, any> = { ...baseProfile };
      if (fs.existsSync(rootConfigPath)) {
        try {
          const cur = JSON.parse(fs.readFileSync(rootConfigPath, 'utf-8'));
          // Preserve user preferences while enforcing curated profile architectural invariants
          activeConfig = {
            ...cur,
            ...(baseProfile.engine ? { engine: baseProfile.engine } : {}),
            ...(baseProfile.api ? { api: baseProfile.api } : {}),
            ...(baseProfile.reverseZ !== undefined ? { reverseZ: baseProfile.reverseZ } : {}),
            ...(baseProfile.rowMajorMatrices !== undefined ? { rowMajorMatrices: baseProfile.rowMajorMatrices } : {}),
            ...(baseProfile.matrixPrecision ? { matrixPrecision: baseProfile.matrixPrecision } : {}),
            ...(baseProfile.srgbCorrection !== undefined ? { srgbCorrection: baseProfile.srgbCorrection } : {}),
            ...(baseProfile.depthSubmission !== undefined ? { depthSubmission: baseProfile.depthSubmission } : {}),
            ...(baseProfile.contrast !== undefined ? { contrast: baseProfile.contrast } : (cur.contrast !== undefined ? { contrast: cur.contrast } : {})),
            ...(baseProfile.saturation !== undefined ? { saturation: baseProfile.saturation } : (cur.saturation !== undefined ? { saturation: cur.saturation } : {})),
            ...(baseProfile.brightness !== undefined ? { brightness: baseProfile.brightness } : (cur.brightness !== undefined ? { brightness: cur.brightness } : {})),
          };
        } catch {}
      }

      activeSessionConfig = activeConfig;

      if (Object.keys(activeConfig).length > 0) {
        try {
          fs.writeFileSync(rootConfigPath, JSON.stringify(activeConfig, null, 2), 'utf-8');
        } catch (e) {}

        for (const d of targetDirs) {
          const destCfg = path.join(d, 'vrinject.json');
          if (destCfg !== rootConfigPath) {
            try {
              fs.writeFileSync(destCfg, JSON.stringify(activeConfig, null, 2), 'utf-8');
            } catch (e) {}
          }
        }

        // Clean up any stale global vrinject.json in updates directory to avoid cross-game configuration contamination
        try {
          const staleStageCfg = path.join(updatesDir, 'vrinject.json');
          if (fs.existsSync(staleStageCfg)) {
            fs.unlinkSync(staleStageCfg);
          }
        } catch (e) {}
      }
    } catch (error) {
      console.warn('Non-fatal error copying shader/model assets to target directory:', error);
    }

    activeTargetExeName = targetExeName;
    activeTargetPid = 0;
    activeGameId = validId;
    cancelInjectionFlag = false;

    emitLogLine(event, `[Injector] Waiting up to 120s for: ${targetExeName}`);

    let targetPid = 0;
    for (let attempts = 0; attempts < 240; attempts++) {
      if (cancelInjectionFlag) {
        stopActiveLogWatch();
        return { success: false, cancelled: true, message: 'Cancelled by user.' };
      }

      await new Promise(resolve => setTimeout(resolve, 500));
      try {
        const { stdout } = await execFileAsync(
          'tasklist.exe',
          ['/fi', `IMAGENAME eq ${targetExeName}`, '/fo', 'csv', '/nh'],
          { encoding: 'utf-8', timeout: 2000, killSignal: 'SIGKILL', windowsHide: true }
        );

        const candidates: Array<{ pid: number; memory: number }> = [];
        for (const line of stdout.split('\n')) {
          if (!line.trim() || line.startsWith('INFO:')) continue;
          const parts = line.split('","');
          if (parts.length < 5) continue;
          const exeName = parts[0].replace(/"/g, '').toLowerCase();
          const pid = Number.parseInt(parts[1].replace(/"/g, ''), 10);
          const memory = Number.parseInt(
            parts[4].replace(/"/g, '').replace(/,/g, '').replace(/\s*K\s*$/i, ''),
            10
          );
          // Filter out processes with invalid PID or negligible memory (e.g. zombie processes < 5MB)
          if (exeName === targetExeName && Number.isSafeInteger(pid) && memory > 5000) {
            candidates.push({ pid, memory: Number.isFinite(memory) ? memory : 0 });
          }
        }

        // Prefer shipping binary in subfolders (e.g. Phoenix/Binaries/Win64) or processes with genuine game memory
        let selectedCandidate: { pid: number; memory: number; path: string } | null = null;
        for (const candidate of candidates) {
          const processPath = await getProcessPath(candidate.pid).catch(() => '');
          if (!processPath) continue;

          const isInsideInstall = processPath.toLowerCase().startsWith(path.resolve(installPath).toLowerCase());
          if (!isInsideInstall) continue;

          const lowerPath = processPath.toLowerCase();
          const isShippingBinary = lowerPath.includes('binaries') || lowerPath.includes('shipping');
          
          // If this is the heavy shipping binary or has > 50MB memory, select it immediately!
          if (isShippingBinary || candidate.memory > 50000) {
            selectedCandidate = { ...candidate, path: processPath };
            break;
          }

          // Otherwise keep as fallback (in case it's an indie title without subfolders)
          if (!selectedCandidate) {
            selectedCandidate = { ...candidate, path: processPath };
          }
        }

        // Resilient Fallback: If getProcessPath was blocked by Windows token/security permissions
        // (e.g. UAC / admin token restrictions on Steam titles like Sekiro), we already verified that
        // tasklist.exe found targetExeName. Select the candidate with primary game memory.
        if (!selectedCandidate && candidates.length > 0) {
          const sorted = [...candidates].sort((a, b) => b.memory - a.memory);
          const best = sorted[0];
          if (best.memory > 30000 || attempts >= 20) {
            selectedCandidate = {
              pid: best.pid,
              memory: best.memory,
              path: path.join(targetExeDir, targetExeName),
            };
          }
        }

        if (selectedCandidate) {
          // If it's a small stub (< 50MB) and not in Binaries, wait up to 15 seconds to let the real game spawn
          const lowerPath = selectedCandidate.path.toLowerCase();
          const isShippingBinary = lowerPath.includes('binaries') || lowerPath.includes('shipping');
          if (!isShippingBinary && selectedCandidate.memory < 50000 && attempts < 30) {
            // Keep waiting for the real game process to spawn!
            continue;
          }

          targetPid = selectedCandidate.pid;
          activeTargetPid = selectedCandidate.pid;
          targetExeDir = path.dirname(selectedCandidate.path);
          if (targetExeDir && path.resolve(targetExeDir) !== path.resolve(installPath)) {
            const targetDirLog = path.join(targetExeDir, 'vrinject.log');
            startWatchingLogFile(targetDirLog, event);
          }
          break;
        }
        if (targetPid > 0) break;
      } catch {
        // Process may not have started yet.
      }
    }

    if (!targetPid) {
      stopActiveLogWatch();
      sendDiscordTelemetry({
        gameId: validId,
        gameName: targetExeName || validId,
        status: 'error',
        message: 'Game executable not found or closed immediately before injection.',
        config: activeSessionConfig,
        logFilePath: currentSessionLogPath || latestSessionLogPath,
      }).catch(() => {});
      return { success: false, message: 'Game executable not found or closed immediately' };
    }

    const bundledDll = resolveWithinRoot(canonicalBinSourceDir, 'vrinject.dll');
    const canonicalDll = fs.existsSync(bundledDll)
      ? canonicalExistingPath(bundledDll, 'file')
      : bundledDll;
    const otaDll = path.join(app.getPath('userData'), 'updates', 'vrinject.dll');
    const sourceDll = pickPreferredAsset(canonicalDll, otaDll, 100000);
    let dllTarget = resolveWithinRoot(targetExeDir, 'vrinject.dll');
    
    // Check if target directory DLL is up-to-date with the source binary.
    // If the game was running and locked vrinject.dll, copying may have failed.
    // Fall back to injecting sourceDll directly so SHA-256 verification succeeds.
    try {
      if (fs.existsSync(sourceDll)) {
        if (!fs.existsSync(dllTarget) || fs.statSync(dllTarget).mtimeMs < fs.statSync(sourceDll).mtimeMs) {
          try {
            fs.copyFileSync(sourceDll, dllTarget);
          } catch {
            dllTarget = sourceDll;
          }
        }
      }
    } catch {
      dllTarget = sourceDll;
    }

    if (!fs.existsSync(cliSource)) {
      stopActiveLogWatch();
      return {
        success: false,
        message: `Injector CLI binary not found at '${cliSource}'. Please reinstall or run updates.`,
      };
    }

    const escapePs = (str: string) => str.replace(/'/g, "''");
    const updatesDir = path.join(app.getPath('userData'), 'updates');
    const copySources = (!app.isPackaged ? [canonicalBinSourceDir, updatesDir] : [updatesDir, canonicalBinSourceDir])
      .filter(d => fs.existsSync(d)).join(';');
    const effectiveCopySrc = copySources || canonicalBinSourceDir;
    const innerScript =
      `$env:NEXVR_AUTH_TOKEN = '${escapePs(process.env.NEXVR_AUTH_TOKEN || '')}'; ` +
      `& '${escapePs(cliSource)}' --pid ${targetPid} --dll '${escapePs(dllTarget)}' ` +
      `--copy-src '${escapePs(effectiveCopySrc)}' --copy-dst '${escapePs(targetExeDir)}' ` +
      `*>&1 | Out-File -LiteralPath '${escapePs(logPath)}' -Append -Encoding utf8; ` +
      `if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }`;
    const base64Inner = Buffer.from(innerScript, 'utf16le').toString('base64');
    const outerScript =
      `$p = Start-Process powershell -Verb RunAs -Wait -PassThru -WindowStyle Hidden ` +
      `-ArgumentList "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", ` +
      `"-EncodedCommand", "${base64Inner}"; ` +
      `if ($p.ExitCode -ne 0) { exit $p.ExitCode }`;
    const base64Outer = Buffer.from(outerScript, 'utf16le').toString('base64');

    return await new Promise<InjectResult>((resolve) => {
      let resolved = false;
      const finish = (result: InjectResult) => {
        if (resolved) return;
        resolved = true;
        clearTimeout(timeout);
        if (!result.success) stopActiveLogWatch();
        resolve(result);
      };

      const timeout = setTimeout(() => {
        finish({
          success: false,
          message: 'Injector timed out or was blocked by Anti-Cheat. (UAC timeout?)',
        });
      }, 180000);

      child_process.execFile(
        'powershell.exe',
        ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', base64Outer],
        {
          timeout: 180000,
          killSignal: 'SIGKILL',
          cwd: installPath,
          windowsHide: true,
          env: {
            ...process.env,
            NEXVR_AUTH_TOKEN: process.env.NEXVR_AUTH_TOKEN || '',
            NEXVR_CLI: cliSource,
            NEXVR_PID: String(targetPid),
            NEXVR_DLL: dllTarget,
            NEXVR_COPY_SRC: canonicalBinSourceDir,
            NEXVR_COPY_DST: targetExeDir,
            NEXVR_LOG: logPath,
          },
        },
        (error) => {
          if (error) {
            sendDiscordTelemetry({
              gameId: validId,
              gameName: targetExeName || validId,
              status: 'error',
              message: `Injection CLI failed: ${error.message}`,
              config: activeSessionConfig,
              logFilePath: currentSessionLogPath || latestSessionLogPath,
            }).catch(() => {});
            finish({ success: false, message: `CLI failed: ${error.message}` });
          } else {
            activeSessionStartTime = Date.now();
            sendDiscordTelemetry({
              gameId: validId,
              gameName: targetExeName,
              status: 'started',
              message: `NexVR Engine successfully injected into ${targetExeName} (PID ${targetPid}).`,
              config: activeSessionConfig,
              logFilePath: currentSessionLogPath || latestSessionLogPath,
            }).catch(() => {});
            finish({ success: true, message: 'Deployed successfully', pid: targetPid });
          }
        }
      );
    });
  } catch (error: any) {
    stopActiveLogWatch();
    sendDiscordTelemetry({
      gameId: activeGameId || 'error_session',
      gameName: activeTargetExeName,
      status: 'error',
      message: `Exception during injection: ${error?.message || String(error)}`,
      config: activeSessionConfig,
      logFilePath: currentSessionLogPath || latestSessionLogPath,
    }).catch(() => {});
    return { success: false, message: error?.message || String(error) };
  } finally {
    injectionInProgress = false;
  }
});

ipcMain.handle('inject:monitor', async (event, pid: number) => {
  assertTrustedIpcSender(event);
  if (!Number.isSafeInteger(pid) || pid <= 0 || pid !== activeTargetPid) {
    throw new Error('Invalid process monitor request');
  }

  // Max 43200 iterations (approx 24 hours at 2s per check)
  for (let i = 0; i < 43200; i++) {
    try {
      const { stdout } = await execFileAsync(
        'tasklist.exe',
        ['/fi', `PID eq ${pid}`, '/fo', 'csv', '/nh'],
        { encoding: 'utf-8', timeout: 2000, killSignal: 'SIGKILL', windowsHide: true }
      );
      if (!stdout.includes(`"${pid}"`)) break;
    } catch {
      break;
    }
    await new Promise(resolve => setTimeout(resolve, 2000));
  }

  const durationSec = activeSessionStartTime > 0 ? Math.floor((Date.now() - activeSessionStartTime) / 1000) : 0;
  const finishedGameId = activeGameId;
  const finishedExeName = activeTargetExeName;
  const logFileToSend = currentSessionLogPath || latestSessionLogPath;

  stopActiveLogWatch();
  activeTargetPid = 0;
  activeTargetExeName = '';
  activeGameId = '';
  activeSessionStartTime = 0;

  sendDiscordTelemetry({
    gameId: finishedGameId || 'completed_session',
    gameName: finishedExeName,
    status: 'completed',
    message: `Game session ended normally. Total playtime: ${durationSec}s.`,
    durationSec,
    logFilePath: logFileToSend,
  }).catch(() => {});
});

// 1-Click "Disable VR / Play Flat" Uninstaller
ipcMain.handle('inject:uninstall', async (event, id: string) => {
  assertTrustedIpcSender(event);
  const validId = validateGameId(id);
  const registeredInstallPath = gamePathsMap[validId];
  if (!registeredInstallPath) return { success: false, message: 'Game path not found' };
  const installPath = canonicalExistingPath(registeredInstallPath, 'directory');

  try {
    let targetExeDir = installPath;
    const registeredExe = gameExeMap[validId];
    if (registeredExe) {
      targetExeDir = path.dirname(canonicalExistingPath(registeredExe, 'file'));
    } else {
      // Locate directory where vrinject.dll was deployed
      const scanForVrinject = (dir: string, depth = 0): string | null => {
        if (depth > 6) return null;
        try {
          const entries = fs.readdirSync(dir, { withFileTypes: true });
          for (const e of entries) {
            if (e.isFile() && e.name.toLowerCase() === 'vrinject.dll') return dir;
            if (e.isDirectory() && !e.name.toLowerCase().includes('support')) {
              const res = scanForVrinject(path.join(dir, e.name), depth + 1);
              if (res) return res;
            }
          }
        } catch {}
        return null;
      };
      const foundDir = scanForVrinject(installPath);
      if (foundDir) targetExeDir = foundDir;
    }

    // Remove deployed VR injection files
    const filesToRemove = ['vrinject.dll', 'onnxruntime.dll', 'DirectML.dll', 'openxr_loader.dll', 'vrinject.log'];
    let removedCount = 0;
    for (const f of filesToRemove) {
      const p = path.join(targetExeDir, f);
      if (fs.existsSync(p)) {
        try {
          fs.unlinkSync(p);
          removedCount++;
        } catch {}
      }
    }
    const shadersDir = path.join(targetExeDir, 'shaders');
    if (fs.existsSync(shadersDir)) {
      try {
        fs.rmSync(shadersDir, { recursive: true, force: true });
        removedCount++;
      } catch {}
    }

    console.info(`[Injector] Uninstalled VR mod from: ${targetExeDir} (${removedCount} files removed)`);
    return {
      success: true,
      message: 'VR Mod uninstalled successfully. Game is now restored to original flat screen mode.'
    };
  } catch (err: any) {
    return { success: false, message: `Failed to uninstall VR mod: ${err.message}` };
  }
});

