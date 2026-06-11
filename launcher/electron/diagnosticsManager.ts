import { app, shell, ipcMain } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import * as util from 'util';
import { VRStatus } from '../src/types';
import { gamePathsMap } from './utils';
import { activeGameId } from './injectionManager';

const execAsync = util.promisify(child_process.exec);

ipcMain.handle('vr:status', async (): Promise<VRStatus> => {
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
      const regOut = child_process.execSync(`reg query "HKLM\\SOFTWARE\\Khronos\\OpenXR\\1" /v "ActiveRuntime"`, { encoding: 'utf-8', stdio: 'pipe' });
      const match = regOut.match(/ActiveRuntime\s+REG_SZ\s+(.+)/i);
      if (match) {
        const rtPath = match[1].toLowerCase();
        if (rtPath.includes('oculus')) {
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

ipcMain.handle('log:export', async (_, lines: unknown) => {
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
        const cfgPath = path.join(installPath, 'vrinject.json');
        if (fs.existsSync(cfgPath)) {
          fs.copyFileSync(cfgPath, path.join(stagingDir, 'vrinject.json'));
        }
        
        const files = fs.readdirSync(installPath);
        for (const file of files) {
          if (file.toLowerCase().endsWith('.dmp')) {
            fs.copyFileSync(path.join(installPath, file), path.join(stagingDir, file));
          }
        }
      } catch(e) {}
    }
  }

  const zipPath = path.join(
    app.getPath('desktop'),
    `NexVR_Diag_Report_${Date.now()}.zip`
  );

  const psCommand = `Compress-Archive -Path "${stagingDir}\\*" -DestinationPath "${zipPath}" -Force`;
  await execAsync(`powershell -NoProfile -Command "${psCommand}"`);

  try {
    fs.rmSync(stagingDir, { recursive: true, force: true });
  } catch(e) {}

  return { success: true, path: zipPath };
});

ipcMain.handle('shell:openExternal', async (_, url: string) => {
  await shell.openExternal(url);
});

ipcMain.handle('utils:openConfig', async (_, id: string) => {
  const installPath = gamePathsMap[id];
  if (installPath) {
    const cfgPath = path.join(installPath, 'vrinject.json');
    if (fs.existsSync(cfgPath)) shell.openPath(cfgPath);
  }
});

ipcMain.handle('utils:openLog', async (_, id: string) => {
  const installPath = gamePathsMap[id];
  if (installPath) {
    const logPath = path.join(installPath, 'vrinject.log');
    if (fs.existsSync(logPath)) shell.openPath(logPath);
  }
});
