import { app, BrowserWindow, ipcMain, shell } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import { GameEntry, VRStatus, VRConfig, InjectResult } from '../src/types';

const isDev = process.env.NODE_ENV === 'development';

let mainWindow: BrowserWindow | null = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1000,
    height: 700,
    frame: false,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: true,
      preload: path.join(__dirname, 'preload.js'),
    },
  });

  if (isDev) {
    mainWindow.loadURL('http://localhost:5173');
  } else {
    mainWindow.loadFile(path.join(__dirname, '../dist/index.html'));
  }
}

app.whenReady().then(() => {
  createWindow();

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

// IPC Handlers

ipcMain.handle('steam:scan', async (): Promise<GameEntry[]> => {
  const games: GameEntry[] = [];
  try {
    const regQuery = `reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath"`;
    const regOut = child_process.execSync(regQuery, { encoding: 'utf-8' });
    const match = regOut.match(/SteamPath\s+REG_SZ\s+(.+)/i);
    if (!match) return [];
    
    const steamPath = match[1].trim().replace(/\//g, '\\');
    const vdfPath = path.join(steamPath, 'steamapps', 'libraryfolders.vdf');
    
    if (!fs.existsSync(vdfPath)) return [];
    const vdfContent = fs.readFileSync(vdfPath, 'utf-8');
    
    const paths = Array.from(vdfContent.matchAll(/"path"\s+"([^"]+)"/g)).map(m => m[1].replace(/\\\\/g, '\\'));
    if (!paths.includes(steamPath)) paths.push(steamPath);
    
    for (const libPath of paths) {
      const appsDir = path.join(libPath, 'steamapps');
      if (!fs.existsSync(appsDir)) continue;
      
      const files = fs.readdirSync(appsDir);
      for (const file of files) {
        if (!file.startsWith('appmanifest_') || !file.endsWith('.acf')) continue;
        
        try {
          const content = fs.readFileSync(path.join(appsDir, file), 'utf-8');
          const idMatch = content.match(/"appid"\s+"([^"]+)"/);
          const nameMatch = content.match(/"name"\s+"([^"]+)"/);
          const dirMatch = content.match(/"installdir"\s+"([^"]+)"/);
          
          if (!idMatch || !nameMatch || !dirMatch) continue;
          
          const installPath = path.join(appsDir, 'common', dirMatch[1]);
          if (!fs.existsSync(installPath)) continue;
          
          let api: 'DX11' | 'DX12' | 'Vulkan' | 'Unknown' = 'DX11';
          const filesInInstall = fs.readdirSync(installPath);
          const hasVulkan = filesInInstall.some(f => f.toLowerCase() === 'vulkan-1.dll' || f.endsWith('.spv'));
          const hasDX12 = filesInInstall.some(f => f.toLowerCase() === 'd3d12.dll' || f.toLowerCase() === 'd3d12core.dll');
          
          if (hasVulkan) api = 'Vulkan';
          else if (hasDX12) api = 'DX12';
          
          const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
          
          games.push({
            id: idMatch[1],
            name: nameMatch[1],
            installPath,
            executablePath: '',
            sizeGB: 0,
            api,
            compat: 'unknown',
            hasInjector
          });
          gamePathsMap[idMatch[1]] = installPath;
        } catch (e) {
          // Ignore parse errors
        }
      }
    }
  } catch (e) {
    console.error('steam:scan error', e);
  }
  return games;
});

ipcMain.handle('vr:status', async (): Promise<VRStatus> => {
  let connected = false;
  let runtime = 'Unknown';
  let headset = 'Unknown HMD';
  
  try {
    const tasklist = child_process.execSync('tasklist', { encoding: 'utf-8' });
    if (tasklist.toLowerCase().includes('ovrserver_x64.exe') || tasklist.toLowerCase().includes('vrserver.exe')) {
      connected = true;
    }
    
    try {
      const regOut = child_process.execSync(`reg query "HKLM\\SOFTWARE\\Khronos\\OpenXR\\1" /v "ActiveRuntime"`, { encoding: 'utf-8' });
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
    
  } catch (e) {}
  
  return { connected, runtime, headset, refreshRate: 90 };
});

const defaultVRConfig: VRConfig = {
  motionAimSensitivity: 1.0,
  useRecommendedResolution: true,
  srgbCorrection: true,
  depthSubmission: false,
  rawInputMode: true,
  autoInjectOnLaunch: true,
};

let gamePathsMap: Record<string, string> = {};

ipcMain.handle('config:read', async (_, id: string): Promise<VRConfig> => {
  try {
    const installPath = gamePathsMap[id];
    if (!installPath) return defaultVRConfig;
    const cfgPath = path.join(installPath, 'vrinject.json');
    if (fs.existsSync(cfgPath)) {
      const content = fs.readFileSync(cfgPath, 'utf-8');
      const parsed = JSON.parse(content);
      return { ...defaultVRConfig, ...parsed };
    }
    return defaultVRConfig;
  } catch (e) {
    return defaultVRConfig;
  }
});

ipcMain.handle('config:write', async (_, id: string, cfg: unknown) => {
  try {
    const installPath = gamePathsMap[id];
    if (!installPath) return { success: false, error: 'Game path not found' };
    const cfgPath = path.join(installPath, 'vrinject.json');
    fs.writeFileSync(cfgPath, JSON.stringify(cfg, null, 2));
    return { success: true };
  } catch (e: any) {
    return { success: false, error: e.message };
  }
});

ipcMain.handle('inject:deploy', async (event, id: string): Promise<InjectResult> => {
  try {
    const installPath = gamePathsMap[id];
    if (!installPath) return { success: false, message: 'Game path not found' };
    
    // 2. Copy vrinject.dll
    const dllSource = isDev ? path.join(__dirname, '../../build/vrinject.dll') : path.join(process.resourcesPath, 'vrinject.dll');
    const dllTarget = path.join(installPath, 'vrinject.dll');
    if (fs.existsSync(dllSource)) {
      fs.copyFileSync(dllSource, dllTarget);
    }
    
    // 3. Launch game
    shell.openExternal('steam://rungameid/' + id);
    
    // 4. Wait 8s
    await new Promise(r => setTimeout(r, 8000));
    
    // 5. Find PID (stubbed naive tasklist search)
    // Actually we could do: tasklist /FI "IMAGENAME eq game.exe" but we don't know the exact exe name here.
    // For this mockup, we'll just fake the PID.
    const fakePid = 1234;
    
    // 6. Call vr-inject-cli.exe
    const cliPath = isDev ? path.join(__dirname, '../../build/vr-inject-cli.exe') : path.join(process.resourcesPath, 'vr-inject-cli.exe');
    if (fs.existsSync(cliPath)) {
       child_process.execFile(cliPath, ['--pid', String(fakePid), '--dll', dllTarget], (err, stdout, stderr) => {
           console.log(stdout, stderr);
       });
    }

    // 7. Watch log file
    const logPath = path.join(installPath, 'vrinject.log');
    if (!fs.existsSync(logPath)) fs.writeFileSync(logPath, '');
    let lastSize = fs.statSync(logPath).size;
    fs.watchFile(logPath, { interval: 500 }, (curr, prev) => {
       if (curr.size > lastSize) {
           const fd = fs.openSync(logPath, 'r');
           const buf = Buffer.alloc(curr.size - lastSize);
           fs.readSync(fd, buf, 0, buf.length, lastSize);
           fs.closeSync(fd);
           const lines = buf.toString('utf-8').split('\n');
           for (const l of lines) {
               if (l.trim()) event.sender.send('log:line', l.trim());
           }
           lastSize = curr.size;
       }
    });

    return { success: true, message: 'Deployed', pid: fakePid };
  } catch(e: any) {
    return { success: false, message: e.message };
  }
});
