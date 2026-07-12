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

const execFileAsync = util.promisify(child_process.execFile);
const isDev = !app.isPackaged;

export let cancelInjectionFlag = false;
export let activeTargetExeName = '';
export let activeTargetPid = 0;
export let activeGameId = '';

const injectRateLimits: Record<string, number[]> = {};
let injectionInProgress = false;
let activeLogPath = '';

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
  const command = 'param([int]$PidArg) (Get-Process -Id $PidArg -ErrorAction Stop).Path';
  const { stdout } = await execFileAsync(
    'powershell.exe',
    ['-NoProfile', '-NonInteractive', '-Command', command, String(pid)],
    { encoding: 'utf-8', timeout: 3000, windowsHide: true }
    
  );
  return stdout.trim();
}

function stopActiveLogWatch(): void {
  if (activeLogPath) fs.unwatchFile(activeLogPath);
  activeLogPath = '';
}

ipcMain.handle('inject:cancel', async (event) => {
  assertTrustedIpcSender(event);
  cancelInjectionFlag = true;

  const pid = activeTargetPid;
  if (pid > 0) {
    await terminatePid(pid);
    setTimeout(() => void terminatePid(pid, true), 2000);
  }
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

    stopActiveLogWatch();
    const logPath = safeGamePath(installPath, 'vrinject.log');
    fs.writeFileSync(logPath, '');
    activeLogPath = logPath;
    let lastSize = 0;

    fs.watchFile(logPath, { interval: 500 }, (curr) => {
      if (curr.size < lastSize) lastSize = 0;
      if (curr.size <= lastSize) return;

      try {
        const fd = fs.openSync(logPath, 'r');
        try {
          const length = Math.min(curr.size - lastSize, 1024 * 1024);
          const buf = Buffer.alloc(length);
          const bytesRead = fs.readSync(fd, buf, 0, length, lastSize);
          lastSize += bytesRead;
          for (const line of buf.subarray(0, bytesRead).toString('utf-8').split('\n')) {
            if (line.trim()) event.sender.send('log:line', line.trim().slice(0, 2000));
          }
        } finally {
          fs.closeSync(fd);
        }
      } catch {
        // The monitored process may rotate or temporarily lock the log.
      }
    });

    const binSourceDir = isDev ? path.join(__dirname, '../../../build/bin') : process.resourcesPath;
    const canonicalBinSourceDir = canonicalExistingPath(binSourceDir, 'directory');
    const cliSource = canonicalExistingPath(
      resolveWithinRoot(canonicalBinSourceDir, 'vr-inject-cli.exe'),
      'file'
    );
    const shadersSource = resolveWithinRoot(canonicalBinSourceDir, 'shaders');
    const modelsSource = resolveWithinRoot(canonicalBinSourceDir, 'models');

    if (validId.startsWith('custom_') && gameExeMap[validId]) {
      const customExe = canonicalExistingPath(gameExeMap[validId], 'file');
      if (path.dirname(customExe).toLowerCase() !== installPath.toLowerCase()) {
        throw new Error('Custom executable is outside its registered install directory');
      }
      const child = child_process.spawn(customExe, [], {
        detached: true,
        cwd: installPath,
        stdio: 'ignore',
      });
      child.unref();
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
      if (fs.existsSync(shadersSource)) {
        fs.cpSync(shadersSource, resolveWithinRoot(targetExeDir, 'shaders'), {
          recursive: true,
          force: true,
          dereference: false,
        });
      }
      if (fs.existsSync(modelsSource)) {
        fs.cpSync(modelsSource, resolveWithinRoot(targetExeDir, 'models'), {
          recursive: true,
          force: true,
          dereference: false,
        });
      }
    } catch (error) {
      console.error('Failed to copy shader/model assets to target directory:', error);
    }

    activeTargetExeName = targetExeName;
    activeTargetPid = 0;
    activeGameId = validId;
    cancelInjectionFlag = false;

    event.sender.send('log:line', `[Injector] Waiting up to 120s for: ${targetExeName}`);

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
          if (exeName === targetExeName && Number.isSafeInteger(pid)) {
            candidates.push({ pid, memory: Number.isFinite(memory) ? memory : 0 });
          }
        }

        candidates.sort((a, b) => b.memory - a.memory);
        for (const candidate of candidates) {
          const processPath = await getProcessPath(candidate.pid).catch(() => '');
          if (
            processPath &&
            path.dirname(processPath).toLowerCase() === path.resolve(targetExeDir).toLowerCase()
          ) {
            targetPid = candidate.pid;
            activeTargetPid = candidate.pid;
            break;
          }
        }
        if (targetPid > 0) break;
      } catch {
        // Process may not have started yet.
      }
    }

    if (!targetPid) {
      stopActiveLogWatch();
      return { success: false, message: 'Game executable not found or closed immediately' };
    }

    const dllTarget = resolveWithinRoot(targetExeDir, 'vrinject.dll');
    const escapePs = (str: string) => str.replace(/'/g, "''");
    const innerScript =
      `$env:NEXVR_AUTH_TOKEN = '${escapePs(process.env.NEXVR_AUTH_TOKEN || '')}'; ` +
      `& '${escapePs(cliSource)}' --pid ${targetPid} --dll '${escapePs(dllTarget)}' ` +
      `--copy-src '${escapePs(canonicalBinSourceDir)}' --copy-dst '${escapePs(targetExeDir)}' ` +
      `*>&1 | Out-File -LiteralPath '${escapePs(logPath)}' -Append -Encoding utf8`;
    const base64Inner = Buffer.from(innerScript, 'utf16le').toString('base64');
    const outerScript =
      `Start-Process powershell -Verb RunAs -Wait -WindowStyle Hidden ` +
      `-ArgumentList "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", ` +
      `"-EncodedCommand", "${base64Inner}"`;
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
      }, 60000);

      child_process.execFile(
        'powershell.exe',
        ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-EncodedCommand', base64Outer],
        {
          timeout: 60000,
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
            finish({ success: false, message: `CLI failed: ${error.message}` });
          } else {
            finish({ success: true, message: 'Deployed successfully', pid: targetPid });
          }
        }
      );
    });
  } catch (error: any) {
    stopActiveLogWatch();
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

  stopActiveLogWatch();
  activeTargetPid = 0;
  activeTargetExeName = '';
  activeGameId = '';
});
