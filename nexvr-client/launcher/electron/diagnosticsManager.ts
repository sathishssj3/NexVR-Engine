import { app, shell, ipcMain } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import * as util from 'util';
import { VRStatus } from '../src/types';
import { assertTrustedIpcSender, gamePathsMap, safeGamePath, validateGameId } from './utils';
import { activeGameId, activeTargetExeName, currentSessionLogPath, latestSessionLogPath, getLogsDir } from './injectionManager';
import { sendDiscordTelemetry } from './telemetryManager';

const execFileAsync = util.promisify(child_process.execFile);

ipcMain.handle('vr:status', async (event): Promise<VRStatus> => {
  assertTrustedIpcSender(event);
  let connected = false;
  let runtime = 'Unknown';
  let headset = 'Unknown HMD';
  
  try {
    // Async tasklist — prevents blocking the main thread (~200-500ms per call)
    const { stdout: tasklistRaw } = await execFileAsync('tasklist.exe', [], {
      encoding: 'utf-8',
      timeout: 3000,
      windowsHide: true,
    });
    const tasklist = tasklistRaw.toLowerCase();
    const isSteamVRRunning = tasklist.includes('vrserver.exe');
    const isOculusRunning = tasklist.includes('ovrserver_x64.exe');
    const isVirtualDesktopRunning = tasklist.includes('virtualdesktop.streamer.exe') || tasklist.includes('virtualdesktop.service.exe');
    const isPicoRunning = tasklist.includes('pico_vr_server.exe') || tasklist.includes('pico connect.exe');
    const isWmrRunning = tasklist.includes('mixedrealityportal.exe');
    
    if (isSteamVRRunning || isOculusRunning || isVirtualDesktopRunning || isPicoRunning || isWmrRunning) {
      connected = true;
    }
    
    try {
      const xrEnv = process.env.XR_RUNTIME_JSON || '';
      // Async registry query — prevents blocking the main thread (~100-200ms per call)
      const { stdout: regOut } = await execFileAsync('reg.exe', [
        'query', 'HKLM\\SOFTWARE\\Khronos\\OpenXR\\1', '/v', 'ActiveRuntime'
      ], { encoding: 'utf-8', timeout: 3000, windowsHide: true });
      const match = regOut.match(/ActiveRuntime\s+REG_(?:EXPAND_)?SZ\s+(.+)/i);
      
      let rtPath = '';
      if (match) {
          rtPath = match[1].toLowerCase();
      }
      if (xrEnv) {
          rtPath = xrEnv.toLowerCase();
      }

      if (rtPath) {
        if (rtPath.includes('meta_openxr_simulator') || rtPath.includes('metaxrsimulator')) {
          runtime = 'Meta XR Simulator';
          headset = 'Meta Quest (Simulated)';
          connected = true;
        } else if (rtPath.includes('oculus')) {
          runtime = 'Oculus';
          headset = 'Meta Quest';
        } else if (rtPath.includes('steamvr')) {
          runtime = 'SteamVR';
          headset = 'SteamVR HMD';
        } else if (rtPath.includes('wmr') || rtPath.includes('mixedreality')) {
          runtime = 'WMR';
          headset = 'WMR Headset';
        }
      }
    } catch(e) {}
    
    if (runtime === 'Unknown') {
        if (isSteamVRRunning) {
            runtime = 'SteamVR';
            headset = 'SteamVR HMD';
        } else if (isOculusRunning) {
            runtime = 'Oculus';
            headset = 'Meta Quest';
        } else if (isVirtualDesktopRunning) {
            runtime = 'Virtual Desktop';
            headset = 'Virtual Desktop XR';
        } else if (isPicoRunning) {
            runtime = 'Pico';
            headset = 'Pico Headset';
        } else if (isWmrRunning) {
            runtime = 'WMR';
            headset = 'WMR Headset';
        }
    }
    
  } catch (e) {}
  
  if (!connected) {
    headset = 'No Headset Connected';
  }
  
  return { connected, runtime, headset, refreshRate: 90 };
});

ipcMain.handle('log:export', async (event, lines: unknown) => {
  assertTrustedIpcSender(event);
  if (!Array.isArray(lines)) throw new Error('Invalid log lines');
  if (lines.length > 10000) throw new Error('Log too large');

  const sanitize = (line: string): string => {
    let s = String(line).substring(0, 500);
    s = s.replace(/[A-Za-z]:\\[^\s"']+\\([^\s"'\\]+)/g, '$1');
    s = s.replace(/\\\\[^\s"']+\\([^\s"'\\]+)/g, '$1');
    s = s.replace(/PID\s+\d+/gi, 'PID [REDACTED]');
    s = s.replace(/pid:\s*\d+/gi, 'pid: [REDACTED]');
    return s;
  };

  const sanitized = lines.map(sanitize);

  const header = [
    '=== NEXVR ENGINE — SESSION LOG EXPORT ===',
    `Exported: ${new Date().toISOString()}`,
    `Lines: ${sanitized.length}`,
    'Note: Paths and PIDs have been redacted for privacy.',
    '============================================',
    '',
  ].join('\n');

  const content = header + sanitized.join('\n');

  const stagingDir = path.join(app.getPath('temp'), `nexvr_export_${Date.now()}`);
  fs.mkdirSync(stagingDir, { recursive: true });
  
  const logTxtPath = path.join(stagingDir, 'nexvr_log.txt');
  await fs.promises.writeFile(logTxtPath, content, 'utf-8');

  if (activeGameId) {
    const installPath = gamePathsMap[activeGameId];
    if (installPath) {
      try {
        const cfgPath = safeGamePath(installPath, 'vrinject.json');
        if (fs.existsSync(cfgPath)) {
          fs.copyFileSync(cfgPath, path.join(stagingDir, 'vrinject.json'));
        }
        
        const files = fs.readdirSync(installPath);
        for (const file of files) {
          const fullPath = safeGamePath(installPath, file);
          const stat = fs.statSync(fullPath);
          if (file.toLowerCase().endsWith('.dmp') && stat.isFile() && stat.size <= 100 * 1024 * 1024) {
            fs.copyFileSync(fullPath, path.join(stagingDir, path.basename(file)));
          }
        }
      } catch(e) {}
    }
  }

  const zipPath = path.join(
    app.getPath('desktop'),
    `NexVR_Diag_Report_${Date.now()}.zip`
  );

  const psCommand = `Compress-Archive -LiteralPath '${stagingDir.replace(/'/g, "''")}\\*' -DestinationPath '${zipPath.replace(/'/g, "''")}' -Force`;
  await execFileAsync('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', psCommand], {
    windowsHide: true,
    timeout: 30000,
  });

  try {
    fs.rmSync(stagingDir, { recursive: true, force: true });
  } catch(e) {}

  return { success: true, path: zipPath };
});

ipcMain.handle('shell:openExternal', async (event, url: string) => {
  assertTrustedIpcSender(event);
  const parsed = new URL(url);
  if (parsed.protocol !== 'https:' && parsed.protocol !== 'http:') {
    throw new Error('External URL is not allowed: protocol must be http or https');
  }
  const allowedHostnames = [
    'github.com',
    'raw.githubusercontent.com',
    'discord.gg',
    'discord.com',
    'nexvr.org',
    'store.steampowered.com',
    'steamcommunity.com',
    'youtube.com',
    'youtu.be',
    'x.com',
    'twitter.com',
  ];
  const isAllowed = allowedHostnames.some(
    domain => parsed.hostname === domain || parsed.hostname.endsWith(`.${domain}`)
  );
  if (!isAllowed) {
    throw new Error(`External URL is not allowed: ${parsed.hostname}`);
  }
  await shell.openExternal(parsed.toString());
});

ipcMain.handle('utils:openConfig', async (event, id: string) => {
  assertTrustedIpcSender(event);
  const installPath = gamePathsMap[validateGameId(id)];
  if (installPath) {
    const cfgPath = safeGamePath(installPath, 'vrinject.json');
    if (fs.existsSync(cfgPath)) shell.openPath(cfgPath);
  }
});

ipcMain.handle('utils:openLog', async (event, id?: string) => {
  assertTrustedIpcSender(event);

  // Candidate 1: Current live session log in userData/logs
  if (currentSessionLogPath && fs.existsSync(currentSessionLogPath)) {
    await shell.openPath(currentSessionLogPath);
    return true;
  }

  // Candidate 2: Latest session log in userData/logs
  const latestLog = latestSessionLogPath || path.join(getLogsDir(), 'latest_session.log');
  if (fs.existsSync(latestLog)) {
    await shell.openPath(latestLog);
    return true;
  }

  // Candidate 3: Game install folder log
  const gameId = id || activeGameId;
  if (gameId) {
    try {
      const installPath = gamePathsMap[validateGameId(gameId)];
      if (installPath) {
        const gameLog = safeGamePath(installPath, 'vrinject.log');
        if (fs.existsSync(gameLog)) {
          await shell.openPath(gameLog);
          return true;
        }
      }
    } catch {}
  }

  // Candidate 4: LocalAppData vrinject.log
  const localAppData = process.env.LOCALAPPDATA || '';
  if (localAppData) {
    const appDataLog = path.join(localAppData, 'VRInject', 'vrinject.log');
    if (fs.existsSync(appDataLog)) {
      await shell.openPath(appDataLog);
      return true;
    }
  }

  // If no log file exists yet, write an initial notice to latest_session.log and open it
  const defaultLog = path.join(getLogsDir(), 'latest_session.log');
  const currentAppVersion = app.getVersion() || '0.1.60';
  fs.writeFileSync(
    defaultLog,
    `=== NexVR Engine [v${currentAppVersion}] — Diagnostic Log ===\nNo active session recorded yet. Launch any game in VR to begin live telemetry streaming.\n`,
    'utf-8'
  );
  await shell.openPath(defaultLog);
  return true;
});

ipcMain.handle('utils:openLogFolder', async (event) => {
  assertTrustedIpcSender(event);
  const logsDir = getLogsDir();
  await shell.openPath(logsDir);
  return true;
});

ipcMain.handle('telemetry:sendReport', async (event, options?: { gameId?: string; userNote?: string }) => {
  assertTrustedIpcSender(event);
  const targetId = options?.gameId || activeGameId || 'manual_session';
  const logPath = currentSessionLogPath || latestSessionLogPath || path.join(getLogsDir(), 'latest_session.log');

  return await sendDiscordTelemetry({
    gameId: targetId,
    gameName: activeTargetExeName || targetId,
    status: 'manual_report',
    message: options?.userNote ? `Tester note: ${options.userNote}` : 'Tester manually submitted diagnostic report from NexVR Launcher.',
    logFilePath: fs.existsSync(logPath) ? logPath : undefined,
  });
});

