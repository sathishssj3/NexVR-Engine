import { app, BrowserWindow, ipcMain, shell, dialog } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import * as util from 'util';
import { GameEntry, VRStatus, VRConfig, InjectResult } from '../src/types';

const execAsync = util.promisify(child_process.exec);

process.env['ELECTRON_DISABLE_SECURITY_WARNINGS'] = 'true';

const isDev = !app.isPackaged;

app.disableHardwareAcceleration();

process.on('uncaughtException', (err) => {
  dialog.showErrorBox('Uncaught Exception', err.stack || err.message || String(err));
});
process.on('unhandledRejection', (reason: any) => {
  dialog.showErrorBox('Unhandled Rejection', reason?.stack || reason?.message || String(reason));
});

let mainWindow: BrowserWindow | null = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1000,
    height: 700,
    minWidth: 900,
    minHeight: 650,
    frame: false,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js'),
    },
  });

  if (isDev && process.env.VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL).catch(err => {
      console.error('loadURL Error:', err);
    });
    mainWindow.webContents.openDevTools();
  } else {
    const htmlPath = path.join(__dirname, '..', '..', 'frontend-dist', 'index.html');
    mainWindow.loadFile(htmlPath).catch(err => {
      dialog.showErrorBox('loadFile Error', String(err) + '\nPath was: ' + htmlPath);
    });
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
const gameExeMap: Record<string, string> = {};

const isIgnoredSoftware = (name: string): boolean => {
  const lower = name.toLowerCase();
  return lower.includes('steamworks common redistributables') ||
         lower.includes('proton') ||
         lower.includes('linux runtime') ||
         lower.includes('dedicated server') ||
         lower.includes('soundtrack') ||
         lower.includes('steamvr') ||
         lower.includes('unreal engine') ||
         lower.includes('fab ue plugin') ||
         lower.includes('quixel bridge') ||
         lower.includes('twinmotion');
};

ipcMain.handle('library:scan', async (): Promise<{ active: GameEntry[], waiting: GameEntry[] }> => {
  const games: GameEntry[] = [];
  const waitingGames: GameEntry[] = [];
  const seenIds = new Set<string>();

  let hiddenIds: string[] = [];
  let ignoredIds: string[] = [];
  let compatList: Record<string, any> = {};
  try {
    const hiddenGamesFile = path.join(app.getPath('userData'), 'hidden_games.json');
    if (fs.existsSync(hiddenGamesFile)) {
      hiddenIds = JSON.parse(fs.readFileSync(hiddenGamesFile, 'utf-8'));
    }
    const ignoredGamesFile = path.join(app.getPath('userData'), 'ignored_games.json');
    if (fs.existsSync(ignoredGamesFile)) {
      ignoredIds = JSON.parse(fs.readFileSync(ignoredGamesFile, 'utf-8'));
    }
    const compatGamesFile = path.join(app.getPath('userData'), 'compat_games.json');
    if (fs.existsSync(compatGamesFile)) {
      compatList = JSON.parse(fs.readFileSync(compatGamesFile, 'utf-8'));
    }
  } catch(e) {}

  try {
    const regQuery = `reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath"`;
    const regOut = child_process.execSync(regQuery, { encoding: 'utf-8' });
    const match = regOut.match(/SteamPath\s+REG_SZ\s+(.+)/i);
    if (!match) return { active: games, waiting: waitingGames };
    
    const steamPath = match[1].trim().replace(/\//g, '\\');
    const vdfPath = path.join(steamPath, 'steamapps', 'libraryfolders.vdf');
    
    if (!fs.existsSync(vdfPath)) return { active: games, waiting: waitingGames };
    const vdfContent = fs.readFileSync(vdfPath, 'utf-8');
    
    const paths = Array.from(vdfContent.matchAll(/"path"\s+"([^"]+)"/g)).map(m => m[1].replace(/\\\\/g, '\\'));
    if (!paths.includes(steamPath)) paths.push(steamPath);
    
    const seenIds = new Set<string>();
    
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
          if (isIgnoredSoftware(nameMatch[1])) continue;
          if (seenIds.has(idMatch[1])) continue;
          seenIds.add(idMatch[1]);
          
          const installPath = path.join(appsDir, 'common', dirMatch[1]);
          if (!fs.existsSync(installPath)) continue;
          
          let api: 'DX11' | 'DX12' | 'Vulkan' | 'Unknown' = 'DX11';
          const filesInInstall = fs.readdirSync(installPath);
          const hasVulkan = filesInInstall.some(f => f.toLowerCase() === 'vulkan-1.dll' || f.endsWith('.spv'));
          const hasDX12 = filesInInstall.some(f => f.toLowerCase() === 'd3d12.dll' || f.toLowerCase() === 'd3d12core.dll');
          
          if (hasVulkan) api = 'Vulkan';
          else if (hasDX12) api = 'DX12';
          
          const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
          
          if (ignoredIds.includes(idMatch[1])) continue;
          
          const entry: GameEntry = {
            id: idMatch[1],
            name: nameMatch[1],
            installPath,
            executablePath: '',
            sizeGB: 0,
            api,
            compat: compatList[idMatch[1]] || 'unknown',
            hasInjector
          };
          gamePathsMap[idMatch[1]] = installPath;

          if (hiddenIds.includes(idMatch[1])) {
            waitingGames.push(entry);
          } else {
            games.push(entry);
          }
        } catch (e) {
          // Ignore parse errors
        }
      }
    }
    
    // Epic Games Scan
    try {
      const manifestsPath = 'C:\\ProgramData\\Epic\\EpicGamesLauncher\\Data\\Manifests';
      if (fs.existsSync(manifestsPath)) {
        const files = fs.readdirSync(manifestsPath);
        for (const file of files) {
          if (!file.endsWith('.item')) continue;
          try {
            const data = fs.readFileSync(path.join(manifestsPath, file), 'utf-8');
            const parsed = JSON.parse(data);
            const installPath = parsed.InstallLocation;
            if (installPath && fs.existsSync(installPath)) {
               let api: 'DX11' | 'DX12' | 'Vulkan' | 'Unknown' = 'DX11';
               try {
                  const filesInInstall = fs.readdirSync(installPath);
                  const hasVulkan = filesInInstall.some(f => f.toLowerCase() === 'vulkan-1.dll' || f.toLowerCase().endsWith('.spv'));
                  const hasDX12 = filesInInstall.some(f => f.toLowerCase() === 'd3d12.dll' || f.toLowerCase() === 'd3d12core.dll');
                  if (hasVulkan) api = 'Vulkan';
                  else if (hasDX12) api = 'DX12';
               } catch(e) {}
               
               const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
               const id = parsed.AppName || parsed.CatalogItemId;
               const dName = parsed.DisplayName;
               if (isIgnoredSoftware(dName)) continue;
               if (ignoredIds.includes(id)) continue;
               
               let iconBase64 = undefined;
               const exeName = parsed.Executable;
               let epicExePath = '';
               if (exeName) {
                 epicExePath = path.join(installPath, exeName);
                 if (fs.existsSync(epicExePath)) {
                   try {
                     const icon = await app.getFileIcon(epicExePath, { size: 'large' });
                     iconBase64 = icon.toDataURL();
                   } catch(e) {}
                 }
               }

               if (!seenIds.has(id)) {
                 seenIds.add(id);
                 const entry: GameEntry = {
                   id: id,
                   name: parsed.DisplayName,
                   installPath,
                   executablePath: epicExePath,
                   sizeGB: 0,
                   api,
                   compat: compatList[id] || 'unknown',
                   hasInjector,
                   iconBase64
                 };
                 gamePathsMap[id] = installPath;
                 if (hiddenIds.includes(id)) {
                   waitingGames.push(entry);
                 } else {
                   games.push(entry);
                 }
               }
            }
          } catch (e) {}
        }
      }
    } catch (e) {}

    // Custom Games Scan
    try {
      const customGamesFile = path.join(app.getPath('userData'), 'custom_games.json');
      if (fs.existsSync(customGamesFile)) {
        const customGamesData = fs.readFileSync(customGamesFile, 'utf-8');
        const customGamesParsed: GameEntry[] = JSON.parse(customGamesData);
        for (const cg of customGamesParsed) {
          if (!seenIds.has(cg.id)) {
            if (ignoredIds.includes(cg.id)) continue;
            seenIds.add(cg.id);
            try {
               if (fs.existsSync(cg.executablePath) && !cg.name.toLowerCase().includes('sekiro')) {
                  const icon = await app.getFileIcon(cg.executablePath, { size: 'large' });
                  cg.iconBase64 = icon.toDataURL();
               }
            } catch(e) {}
            gamePathsMap[cg.id] = cg.installPath;
            gameExeMap[cg.id] = cg.executablePath;
            if (hiddenIds.includes(cg.id)) {
               waitingGames.push(cg);
            } else {
               games.push(cg);
            }
          }
        }
      }
    } catch (e) {}
    
  } catch (e) {
    console.error('library:scan error', e);
  }
  return { active: games, waiting: waitingGames };
});

ipcMain.handle('library:addCustom', async (): Promise<{ success: boolean }> => {
  const { canceled, filePaths } = await dialog.showOpenDialog({
    properties: ['openFile'],
    filters: [{ name: 'Executables', extensions: ['exe'] }]
  });
  
  if (canceled || filePaths.length === 0) return { success: false };
  
  const exePath = filePaths[0];
  const installPath = path.dirname(exePath);
  const name = path.basename(exePath, '.exe');
  const id = 'custom_' + Date.now().toString();
  
  let api: 'DX11' | 'DX12' | 'Vulkan' | 'Unknown' = 'DX11';
  try {
     const filesInInstall = fs.readdirSync(installPath);
     const hasVulkan = filesInInstall.some(f => f.toLowerCase() === 'vulkan-1.dll' || f.toLowerCase().endsWith('.spv'));
     const hasDX12 = filesInInstall.some(f => f.toLowerCase() === 'd3d12.dll' || f.toLowerCase() === 'd3d12core.dll');
     if (hasVulkan) api = 'Vulkan';
     else if (hasDX12) api = 'DX12';
  } catch(e) {}
  
  const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
  
  let iconBase64 = undefined;
  try {
     if (!name.toLowerCase().includes('sekiro')) {
        const icon = await app.getFileIcon(exePath, { size: 'large' });
        iconBase64 = icon.toDataURL();
     }
  } catch(e) {}
  
  const newGame: GameEntry = {
    id,
    name,
    installPath,
    executablePath: exePath,
    sizeGB: 0,
    api,
    compat: 'unknown',
    hasInjector,
    iconBase64
  };
  
  try {
    gameExeMap[id] = exePath;
    const customGamesFile = path.join(app.getPath('userData'), 'custom_games.json');
    let existing = [];
    if (fs.existsSync(customGamesFile)) {
      existing = JSON.parse(fs.readFileSync(customGamesFile, 'utf-8'));
    }
    existing.push(newGame);
    fs.writeFileSync(customGamesFile, JSON.stringify(existing, null, 2));
    return { success: true };
  } catch(e) {
    return { success: false };
  }
});

ipcMain.handle('library:removeGame', async (event, id: string): Promise<{ success: boolean }> => {
  try {
    const hiddenGamesFile = path.join(app.getPath('userData'), 'hidden_games.json');
    let hiddenIds: string[] = [];
    if (fs.existsSync(hiddenGamesFile)) {
      hiddenIds = JSON.parse(fs.readFileSync(hiddenGamesFile, 'utf-8'));
    }
    if (!hiddenIds.includes(id)) {
      hiddenIds.push(id);
      fs.writeFileSync(hiddenGamesFile, JSON.stringify(hiddenIds, null, 2));
    }
    return { success: true };
  } catch(e) {
    return { success: false };
  }
});

ipcMain.handle('library:restoreGame', async (event, id: string): Promise<{ success: boolean }> => {
  try {
    const hiddenGamesFile = path.join(app.getPath('userData'), 'hidden_games.json');
    if (fs.existsSync(hiddenGamesFile)) {
      let hiddenIds: string[] = JSON.parse(fs.readFileSync(hiddenGamesFile, 'utf-8'));
      hiddenIds = hiddenIds.filter(x => x !== id);
      fs.writeFileSync(hiddenGamesFile, JSON.stringify(hiddenIds, null, 2));
    }
    return { success: true };
  } catch(e) {
    return { success: false };
  }
});

ipcMain.handle('library:ignoreGame', async (event, id: string): Promise<{ success: boolean }> => {
  try {
    const hiddenGamesFile = path.join(app.getPath('userData'), 'hidden_games.json');
    if (fs.existsSync(hiddenGamesFile)) {
      let hiddenIds: string[] = JSON.parse(fs.readFileSync(hiddenGamesFile, 'utf-8'));
      hiddenIds = hiddenIds.filter(x => x !== id);
      fs.writeFileSync(hiddenGamesFile, JSON.stringify(hiddenIds, null, 2));
    }
    
    const ignoredGamesFile = path.join(app.getPath('userData'), 'ignored_games.json');
    let ignoredIds: string[] = [];
    if (fs.existsSync(ignoredGamesFile)) {
      ignoredIds = JSON.parse(fs.readFileSync(ignoredGamesFile, 'utf-8'));
    }
    if (!ignoredIds.includes(id)) {
      ignoredIds.push(id);
      fs.writeFileSync(ignoredGamesFile, JSON.stringify(ignoredIds, null, 2));
    }
    
    if (id.startsWith('custom_')) {
      const customGamesFile = path.join(app.getPath('userData'), 'custom_games.json');
      if (fs.existsSync(customGamesFile)) {
        let existing: GameEntry[] = JSON.parse(fs.readFileSync(customGamesFile, 'utf-8'));
        existing = existing.filter(g => g.id !== id);
        fs.writeFileSync(customGamesFile, JSON.stringify(existing, null, 2));
      }
      delete gameExeMap[id];
      delete gamePathsMap[id];
    }
    return { success: true };
  } catch(e) {
    return { success: false };
  }
});

ipcMain.handle('library:restoreIgnoredGames', async (): Promise<{ success: boolean }> => {
  try {
    const ignoredGamesFile = path.join(app.getPath('userData'), 'ignored_games.json');
    const hiddenGamesFile = path.join(app.getPath('userData'), 'hidden_games.json');
    
    if (fs.existsSync(ignoredGamesFile)) {
      const ignoredIds: string[] = JSON.parse(fs.readFileSync(ignoredGamesFile, 'utf-8'));
      if (ignoredIds.length > 0) {
        let hiddenIds: string[] = [];
        if (fs.existsSync(hiddenGamesFile)) {
          hiddenIds = JSON.parse(fs.readFileSync(hiddenGamesFile, 'utf-8'));
        }
        
        // Add all ignored IDs back into the hidden list (waiting list)
        for (const id of ignoredIds) {
          if (!hiddenIds.includes(id)) {
            hiddenIds.push(id);
          }
        }
        
        fs.writeFileSync(hiddenGamesFile, JSON.stringify(hiddenIds, null, 2));
        fs.writeFileSync(ignoredGamesFile, JSON.stringify([], null, 2));
      }
    }
    return { success: true };
  } catch(e) {
    return { success: false };
  }
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

let cancelInjectionFlag = false;
let activeTargetExeName = '';
let activeTargetPid = 0;
let activeGameId = '';

ipcMain.handle('inject:cancel', async () => {
   cancelInjectionFlag = true;
   try {
      // 1. Graceful taskkill
      if (activeTargetPid > 0) {
         await execAsync(`taskkill /PID ${activeTargetPid}`, { timeout: 2000 }).catch(() => {});
         setTimeout(() => execAsync(`taskkill /PID ${activeTargetPid} /F`, { timeout: 2000 }).catch(() => {}), 2000);
      } else if (activeTargetExeName) {
         await execAsync(`taskkill /IM "${activeTargetExeName}"`, { timeout: 2000 }).catch(() => {});
         setTimeout(() => execAsync(`taskkill /IM "${activeTargetExeName}" /F`, { timeout: 2000 }).catch(() => {}), 2000);
      }
      
      // 2. Common bootstrappers
      await execAsync(`taskkill /IM "FallGuys_client.exe"`, { timeout: 2000 }).catch(() => {});
      await execAsync(`taskkill /IM "launcher.exe"`, { timeout: 2000 }).catch(() => {});
      setTimeout(() => execAsync(`taskkill /IM "FallGuys_client.exe" /F`, { timeout: 2000 }).catch(() => {}), 2000);
      setTimeout(() => execAsync(`taskkill /IM "launcher.exe" /F`, { timeout: 2000 }).catch(() => {}), 2000);
      
      // 3. Fallback: Aggressive case-insensitive folder wipe
      const installPath = gamePathsMap[activeGameId];
      if (installPath) {
         const cleanPath = installPath.toLowerCase().replace(/'/g, "''");
         await execAsync(`powershell -NoProfile -Command "$ErrorActionPreference = 'SilentlyContinue'; Get-Process | Where-Object { $_.Path -ne $null -and $_.Path.ToLower().StartsWith('${cleanPath}') } | Stop-Process -Force"`, { timeout: 3000 }).catch(() => {});
      }
   } catch(e) {}
});

ipcMain.handle('inject:deploy', async (event, id: string): Promise<InjectResult> => {
  try {
    const installPath = gamePathsMap[id];
    if (!installPath) return { success: false, message: 'Game path not found' };
    
    // 1. Reset log file
    const logPath = path.join(installPath, 'vrinject.log');
    fs.writeFileSync(logPath, '');
    let lastSize = 0;

    // Watch log file immediately so we capture early errors
    fs.watchFile(logPath, { interval: 500 }, (curr, prev) => {
       if (curr.size > lastSize) {
           try {
             const fd = fs.openSync(logPath, 'r');
             const buf = Buffer.alloc(curr.size - lastSize);
             fs.readSync(fd, buf, 0, buf.length, lastSize);
             fs.closeSync(fd);
             const lines = buf.toString('utf-8').split('\n');
             for (const l of lines) {
                 if (l.trim()) event.sender.send('log:line', l.trim());
             }
             lastSize = curr.size;
           } catch(e) {}
       }
    });

    // 2. Copy vrinject.dll
    const dllSource = isDev ? path.join(__dirname, '../../build/vrinject.dll') : path.join(process.resourcesPath, 'vrinject.dll');
    const dllTarget = path.join(installPath, 'vrinject.dll');
    if (fs.existsSync(dllSource)) {
      fs.copyFileSync(dllSource, dllTarget);
    }
    
    // 3. Launch game
    if (id.startsWith('custom_') && gameExeMap[id]) {
       child_process.spawn(gameExeMap[id], [], { detached: true, cwd: installPath });
    } else if (/^\d+$/.test(id)) {
       shell.openExternal('steam://rungameid/' + id);
    } else {
       shell.openExternal(`com.epicgames.launcher://apps/${id}?action=launch&silent=true`);
    }
    
    // 4. Pre-scan for expected executable if unknown
    let targetExeName = '';
    if (gameExeMap[id]) {
       targetExeName = path.basename(gameExeMap[id]).toLowerCase();
    } else {
       let largestSize = 0;
       const scan = (dir: string) => {
         try {
           const files = fs.readdirSync(dir);
           for (const f of files) {
             const fullPath = path.join(dir, f);
             try {
               const stats = fs.statSync(fullPath);
               if (stats.isDirectory()) {
                 scan(fullPath);
               } else if (f.toLowerCase().endsWith('.exe')) {
                 const lower = f.toLowerCase();
                 if (lower.includes('launcher') || lower.includes('crash') || lower.includes('reporter') || lower.includes('anticheat') || lower.includes('eosbootstrapper') || lower.includes('start_protected_game')) {
                    continue;
                 }
                 if (stats.size > largestSize) {
                    largestSize = stats.size;
                    targetExeName = f.toLowerCase();
                 }
               }
             } catch(e) {}
           }
         } catch(e) {}
       };
       scan(installPath);
    }
    
    if (!targetExeName) {
       fs.unwatchFile(logPath);
       return { success: false, message: 'Could not determine main executable name' };
    }
    
    activeTargetExeName = targetExeName;
    activeTargetPid = 0;
    activeGameId = id;

    // 5. Polling for target PID using rock-solid tasklist
    let targetPid = 0;
    let attempts = 0;
    cancelInjectionFlag = false;
    
    while (attempts < 60) { // Up to 30 seconds
      if (cancelInjectionFlag) {
         fs.unwatchFile(logPath);
         return { success: false, cancelled: true, message: 'Cancelled by user.' };
      }
      
      await new Promise(r => setTimeout(r, 500));
      attempts++;
      
      try {
        const { stdout: out } = await execAsync('tasklist /fo csv /nh', { encoding: 'utf-8', timeout: 2000, killSignal: 'SIGKILL' });
        const lines = out.split('\n');
        
        let foundPid = 0;
        let anyGameProcessRunning = false;
        
        for (const line of lines) {
           if (!line.trim()) continue;
           const parts = line.split('","');
           if (parts.length < 2) continue;
           
           const exeName = parts[0].replace(/"/g, '').toLowerCase();
           const pid = parseInt(parts[1].replace(/"/g, ''), 10);
           
           const baseExeName = exeName.endsWith('.exe') ? exeName.slice(0, -4) : exeName;
           if (exeName === targetExeName || targetExeName.startsWith(baseExeName)) {
              foundPid = pid;
              break;
           }
        }
        
        if (foundPid > 0) {
           // Verify it stays alive for 2 seconds
           await new Promise(r => setTimeout(r, 2000));
           const { stdout: out2 } = await execAsync(`tasklist /fi "PID eq ${foundPid}" /fo csv /nh`, { encoding: 'utf-8', timeout: 2000, killSignal: 'SIGKILL' });
           if (out2.includes(String(foundPid))) {
              targetPid = foundPid;
              activeTargetPid = targetPid;
              break;
           }
        }
      } catch (e) {}
    }
    
    if (!targetPid) {
       fs.unwatchFile(logPath);
       return { success: false, message: 'Game executable not found or closed immediately' };
    }
    
    // 5. Call vr-inject-cli.exe
    const cliPath = isDev ? path.join(__dirname, '../../build/vr-inject-cli.exe') : path.join(process.resourcesPath, 'vr-inject-cli.exe');
    if (!fs.existsSync(cliPath)) {
       fs.unwatchFile(logPath);
       return { success: false, message: 'Injector CLI not found' };
    }
    
    return new Promise((resolve) => {
       let isResolved = false;
       const fallbackTimeout = setTimeout(() => {
           if (!isResolved) {
               isResolved = true;
               fs.unwatchFile(logPath);
               resolve({ success: false, message: 'Injector timed out or was blocked by Anti-Cheat.' });
           }
       }, 10000);

       child_process.execFile(cliPath, ['--pid', String(targetPid), '--dll', dllTarget, '--launcher-key', 'NexVR-Authorized-Launch'], { timeout: 10000, killSignal: 'SIGKILL' }, (err, stdout, stderr) => {
           if (isResolved) return;
           isResolved = true;
           clearTimeout(fallbackTimeout);

           if (stdout.trim()) event.sender.send('log:line', '[Injector CLI] ' + stdout.trim());
           if (stderr.trim()) event.sender.send('log:line', '[Injector CLI Error] ' + stderr.trim());
           
           if (err) {
               fs.unwatchFile(logPath);
               resolve({ success: false, message: `CLI failed: ${err.message}` });
           } else {
               // Leave watcher running to continue streaming real game logs
               resolve({ success: true, message: 'Deployed successfully', pid: targetPid });
           }
       });
    });

  } catch(e: any) {
    return { success: false, message: e.message };
  }
});

ipcMain.handle('inject:monitor', async (_, pid: number) => {
    while (true) {
        try {
            const { stdout } = await execAsync(`tasklist /fi "PID eq ${pid}" /fo csv /nh`, { encoding: 'utf-8', timeout: 2000, killSignal: 'SIGKILL' });
            if (!stdout.includes(String(pid))) {
                break;
            }
        } catch (e) {
            break;
        }
        await new Promise(resolve => setTimeout(resolve, 2000));
    }
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

  // Create staging directory
  const stagingDir = path.join(app.getPath('temp'), `nexvr_export_${Date.now()}`);
  fs.mkdirSync(stagingDir, { recursive: true });
  
  const logTxtPath = path.join(stagingDir, 'nexvr_log.txt');
  await fs.promises.writeFile(logTxtPath, content, 'utf-8');

  // Gather config and dumps if we know the active game
  if (activeGameId) {
    const installPath = gamePathsMap[activeGameId];
    if (installPath) {
      try {
        const cfgPath = path.join(installPath, 'vrinject.json');
        if (fs.existsSync(cfgPath)) {
          fs.copyFileSync(cfgPath, path.join(stagingDir, 'vrinject.json'));
        }
        
        // Grab crash dumps
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

  // Compress using native PowerShell
  const psCommand = `Compress-Archive -Path "${stagingDir}\\*" -DestinationPath "${zipPath}" -Force`;
  await execAsync(`powershell -NoProfile -Command "${psCommand}"`);

  // Cleanup
  try {
    fs.rmSync(stagingDir, { recursive: true, force: true });
  } catch(e) {}

  return { success: true, path: zipPath };
});

ipcMain.handle('shell:openExternal', async (_, url: string) => {
  await shell.openExternal(url);
});

ipcMain.on('window:minimize', () => {
  if (mainWindow) mainWindow.minimize();
});

ipcMain.on('window:maximize', () => {
  if (mainWindow) {
    if (mainWindow.isMaximized()) mainWindow.unmaximize();
    else mainWindow.maximize();
  }
});

ipcMain.on('window:close', () => {
  if (mainWindow) mainWindow.close();
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

