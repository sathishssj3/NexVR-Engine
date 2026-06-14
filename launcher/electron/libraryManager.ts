import { app, dialog, ipcMain } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import { GameEntry } from '../src/types';
import {
  gamePathsMap,
  gameExeMap,
  sanitizeAcfString,
  validateSteamPath,
  isIgnoredSoftware,
  validateGameId
} from './utils';

const isDev = !app.isPackaged;


function detectAPI(dirPath: string, depth = 0): 'DX11' | 'DX12' | 'Vulkan' | 'Unknown' {
  if (depth > 6) return 'Unknown';
  try {
    const files = fs.readdirSync(dirPath, { withFileTypes: true });
    let hasDirs = [];
    for (const file of files) {
      if (file.isFile()) {
        const lower = file.name.toLowerCase();
        if (lower === 'vulkan-1.dll' || lower.endsWith('.spv')) return 'Vulkan';
        if (lower === 'd3d12.dll' || lower === 'd3d12core.dll' || lower === 'dxil.dll') return 'DX12';
      } else if (file.isDirectory()) {
        hasDirs.push(file.name);
      }
    }
    for (const d of hasDirs) {
      const res = detectAPI(path.join(dirPath, d), depth + 1);
      if (res !== 'Unknown') return res;
    }
  } catch(e) {}
  return 'Unknown';
}
let cachedHiddenIds: string[] | null = null;
let cachedIgnoredIds: string[] | null = null;

function getHiddenIds(): string[] {
  if (cachedHiddenIds !== null) return cachedHiddenIds as string[];
  try {
    const p = path.join(app.getPath('userData'), 'hidden_games.json');
    if (fs.existsSync(p)) cachedHiddenIds = JSON.parse(fs.readFileSync(p, 'utf-8'));
    else cachedHiddenIds = [];
  } catch(e) { cachedHiddenIds = []; }
  return cachedHiddenIds as string[];
}

function getIgnoredIds(): string[] {
  if (cachedIgnoredIds !== null) return cachedIgnoredIds as string[];
  try {
    const p = path.join(app.getPath('userData'), 'ignored_games.json');
    if (fs.existsSync(p)) cachedIgnoredIds = JSON.parse(fs.readFileSync(p, 'utf-8'));
    else cachedIgnoredIds = [];
  } catch(e) { cachedIgnoredIds = []; }
  return cachedIgnoredIds as string[];
}

function saveHiddenIds(ids: string[]) {
  cachedHiddenIds = ids;
  fs.writeFileSync(path.join(app.getPath('userData'), 'hidden_games.json'), JSON.stringify(ids, null, 2));
}

function saveIgnoredIds(ids: string[]) {
  cachedIgnoredIds = ids;
  fs.writeFileSync(path.join(app.getPath('userData'), 'ignored_games.json'), JSON.stringify(ids, null, 2));
}


ipcMain.handle('library:scan', async (): Promise<{ active: GameEntry[], waiting: GameEntry[] }> => {
  const games: GameEntry[] = [];
  const waitingGames: GameEntry[] = [];
  const seenIds = new Set<string>();

  let hiddenIds = getHiddenIds();
  let ignoredIds = getIgnoredIds();
  let compatList: Record<string, any> = {};
  try {
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
    if (!validateSteamPath(steamPath)) {
      console.log('[SECURITY] Steam path validation failed');
      return { active: games, waiting: waitingGames };
    }
    const vdfPath = path.join(steamPath, 'steamapps', 'libraryfolders.vdf');
    
    if (!fs.existsSync(vdfPath)) return { active: games, waiting: waitingGames };
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
          
          const idStr = sanitizeAcfString(idMatch[1]);
          const nameStr = sanitizeAcfString(nameMatch[1]);
          const dirStr = sanitizeAcfString(dirMatch[1]);

          if (isIgnoredSoftware(nameStr)) continue;
          if (seenIds.has(idStr)) continue;
          seenIds.add(idStr);
          
          const installPath = path.join(appsDir, 'common', dirStr);
          if (!fs.existsSync(installPath)) continue;
          
          let api = detectAPI(installPath);
          if (api === 'Unknown') api = 'DX11'; // default
          
          const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
          
          if (ignoredIds.includes(idStr)) continue;
          
          const entry: GameEntry = {
            id: idStr,
            name: nameStr,
            installPath,
            executablePath: '',
            sizeGB: 0,
            api,
            compat: compatList[idStr] || 'unknown',
            hasInjector
          };
          gamePathsMap[idStr] = installPath;

          if (hiddenIds.includes(idStr)) {
            waitingGames.push(entry);
          } else {
            games.push(entry);
          }
        } catch (e) {}
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
               let api = detectAPI(installPath);
               if (api === 'Unknown') api = 'DX11';
               
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
    let hiddenIds = getHiddenIds();
    if (!hiddenIds.includes(id)) {
      hiddenIds.push(id);
      saveHiddenIds(hiddenIds);
    }
    return { success: true };
  } catch(e) {
    return { success: false };
  }
});

ipcMain.handle('library:restoreGame', async (event, id: string): Promise<{ success: boolean }> => {
  try {
    let hiddenIds = getHiddenIds();
    hiddenIds = hiddenIds.filter(x => x !== id);
    saveHiddenIds(hiddenIds);
    return { success: true };
  } catch(e) {
    return { success: false };
  }
});

ipcMain.handle('library:ignoreGame', async (event, id: string): Promise<{ success: boolean }> => {
  try {
    let hiddenIds = getHiddenIds();
    hiddenIds = hiddenIds.filter(x => x !== id);
    saveHiddenIds(hiddenIds);
    
    let ignoredIds = getIgnoredIds();
    if (!ignoredIds.includes(id)) {
      ignoredIds.push(id);
      saveIgnoredIds(ignoredIds);
    }
    return { success: true };
  } catch(e) {
    return { success: false };
  }
});

ipcMain.handle('library:restoreIgnoredGames', async (): Promise<{ success: boolean }> => {
  try {
    let ignoredIds = getIgnoredIds();
    if (ignoredIds.length > 0) {
      let hiddenIds = getHiddenIds();
      // Completely restore them to the Active menu
      hiddenIds = hiddenIds.filter(id => !ignoredIds.includes(id));
      saveHiddenIds(hiddenIds);
      saveIgnoredIds([]);
    }
    return { success: true };
  } catch(e) {
    return { success: false };
  }
});
