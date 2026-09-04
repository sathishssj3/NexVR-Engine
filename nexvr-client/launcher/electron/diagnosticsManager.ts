import { app, shell, ipcMain } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import * as util from 'util';
import { VRStatus } from '../src/types';
import { assertTrustedIpcSender, gamePathsMap, safeGamePath, validateGameId } from './utils';
import { activeGameId } from './injectionManager';

const execFileAsync = util.promisify(child_process.execFile);

ipcMain.handle('vr:status', async (event): Promise<VRStatus> => {
  assertTrustedIpcSender(event);
  let connected = false;
  let runtime = 'Unknown';
  let headset = 'Unknown HMD';
  
  try {
    const tasklist = child_process.execSync('tasklist', { encoding: 'utf-8' });
    let isSteamVRRunning = tasklist.toLowerCase().includes('vrserver.exe');
    let isOculusRunning = tasklist.toLowerCase().includes('ovrserver_x64.exe');
    
    if (isSteamVRRunning || isOculusRunning) {
      connected = true;
    }
    
    try {
      const xrEnv = process.env.XR_RUNTIME_JSON || '';
      const regOut = child_process.execSync(`reg query "HKLM\\SOFTWARE\\Khronos\\OpenXR\\1" /v "ActiveRuntime"`, { encoding: 'utf-8', stdio: 'pipe' });
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
        }
    }
    
  } catch (e) {}
  
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
  if (parsed.protocol !== 'https:' || parsed.hostname !== 'github.com') {
    throw new Error('External URL is not allowed');
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

ipcMain.handle('utils:openLog', async (event, id: string) => {
  assertTrustedIpcSender(event);
  const installPath = gamePathsMap[validateGameId(id)];
  if (installPath) {
    const logPath = safeGamePath(installPath, 'vrinject.log');
    if (fs.existsSync(logPath)) shell.openPath(logPath);
  }
});
