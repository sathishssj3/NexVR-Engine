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
  validateGameId,
  assertTrustedIpcSender,
  canonicalExistingPath,
  resolveWithinRoot,
} from './utils';

const isDev = !app.isPackaged;


function inspectExeAPI(exePath: string): 'DX11' | 'DX12' | 'Vulkan' | 'Unknown' {
  try {
    const stat = fs.statSync(exePath);
    if (!stat.isFile() || stat.size < 1024) return 'Unknown';

    // First check directory for telltale runtime DLLs (e.g. amd_fidelityfx_dx12.dll, d3d12core.dll, vulkan-1.dll)
    const exeDir = path.dirname(exePath);
    try {
      const dirFiles = fs.readdirSync(exeDir);
      for (const df of dirFiles) {
        const lower = df.toLowerCase();
        if (lower.includes('dx12') || lower.includes('d3d12core') || lower === 'd3d12.dll') {
          return 'DX12';
        }
        if (lower.includes('vulkan') || lower === 'vulkan-1.dll') {
          return 'Vulkan';
        }
      }
    } catch {}

    // Read up to 256MB in 8MB chunks to search PE import directory and code sections
    const fd = fs.openSync(exePath, 'r');
    const chunkSize = 8 * 1024 * 1024;
    const maxScanBytes = Math.min(stat.size, 256 * 1024 * 1024);
    const buffer = Buffer.alloc(chunkSize);

    let hasD3D12 = false;
    let hasVulkan = false;
    let hasD3D11 = false;

    for (let offset = 0; offset < maxScanBytes; offset += chunkSize) {
      const bytesToRead = Math.min(chunkSize, maxScanBytes - offset);
      const bytesRead = fs.readSync(fd, buffer, 0, bytesToRead, offset);
      if (bytesRead <= 0) break;
      const str = buffer.subarray(0, bytesRead).toString('latin1').toLowerCase();

      if (str.includes('d3d12.dll') || str.includes('d3d12core.dll')) {
        hasD3D12 = true;
        break; // DX12 confirmed
      }
      if (str.includes('vulkan-1.dll')) {
        hasVulkan = true;
      }
      if (str.includes('d3d11.dll')) {
        hasD3D11 = true;
      }
    }
    fs.closeSync(fd);

    if (hasD3D12) return 'DX12';
    if (hasVulkan) return 'Vulkan';
    if (hasD3D11) return 'DX11';
  } catch {}
  return 'Unknown';
}

export function findPrimaryExecutable(dirPath: string, depth = 0): string {
  if (depth > 6) return '';
  let largestExe = '';
  let largestSize = 0;
  try {
    const entries = fs.readdirSync(dirPath, { withFileTypes: true });
    for (const e of entries) {
      if (e.isFile() && e.name.toLowerCase().endsWith('.exe')) {
        const lower = e.name.toLowerCase();
        if (
          !lower.includes('crashreporter') &&
          !lower.includes('crashhandler') &&
          !lower.includes('unrealcefsubprocess') &&
          !lower.includes('unitycrashhandler') &&
          !lower.includes('easyanticheat') &&
          !lower.includes('battleye') &&
          !lower.includes('redist') &&
          !lower.includes('setup') &&
          !lower.includes('launcher') &&
          !lower.includes('epicwebhelper')
        ) {
          try {
            const fullPath = path.join(dirPath, e.name);
            const sz = fs.statSync(fullPath).size;
            if (sz > largestSize) {
              largestSize = sz;
              largestExe = fullPath;
            }
          } catch {}
        }
      }
    }
    for (const e of entries) {
      if (e.isDirectory()) {
        const lower = e.name.toLowerCase();
        if (
          lower !== 'support' &&
          lower !== '_commonredist' &&
          lower !== 'directx' &&
          lower !== 'vc' &&
          lower !== 'installers' &&
          lower !== 'engine'
        ) {
          const subExe = findPrimaryExecutable(path.join(dirPath, e.name), depth + 1);
          if (subExe) {
            try {
              const sz = fs.statSync(subExe).size;
              if (sz > largestSize) {
                largestSize = sz;
                largestExe = subExe;
              }
            } catch {}
          }
        }
      }
    }
  } catch {}
  return largestExe;
}

export function detectAntiCheat(dirPath: string, depth = 0): { hasAntiCheat: boolean; antiCheatName?: string } {
  if (depth > 4) return { hasAntiCheat: false };
  try {
    const files = fs.readdirSync(dirPath, { withFileTypes: true });
    for (const f of files) {
      const lower = f.name.toLowerCase();
      if (lower.includes('easyanticheat') || lower.includes('start_protected_game') || lower === 'eac_server64.dll') {
        return { hasAntiCheat: true, antiCheatName: 'Easy Anti-Cheat (EAC)' };
      }
      if (lower.includes('battleye') || lower.includes('beservice') || lower === 'bedaisy.sys') {
        return { hasAntiCheat: true, antiCheatName: 'BattlEye' };
      }
      if (lower.includes('vgk.sys') || lower === 'vgc.exe') {
        return { hasAntiCheat: true, antiCheatName: 'Riot Vanguard' };
      }
      if (lower.includes('randgrid.sys')) {
        return { hasAntiCheat: true, antiCheatName: 'Ricochet Anti-Cheat' };
      }
      if (lower.includes('denuvo')) {
        return { hasAntiCheat: true, antiCheatName: 'Denuvo Anti-Cheat' };
      }
    }
    for (const f of files) {
      if (f.isDirectory()) {
        const lower = f.name.toLowerCase();
        if (
          lower !== 'support' &&
          lower !== '_commonredist' &&
          lower !== 'directx' &&
          lower !== 'installers' &&
          lower !== 'engine'
        ) {
          const res = detectAntiCheat(path.join(dirPath, f.name), depth + 1);
          if (res.hasAntiCheat) return res;
        }
      }
    }
  } catch {}
  return { hasAntiCheat: false };
}

function detectAPI(dirPath: string, depth = 0): 'DX11' | 'DX12' | 'Vulkan' | 'Unknown' {
  // First, find the primary executable across the whole install folder (including Binaries/Win64)
  const primaryExe = findPrimaryExecutable(dirPath);
  if (primaryExe) {
    const api = inspectExeAPI(primaryExe);
    if (api !== 'Unknown') return api;
  }
  return 'DX11'; // Default for Windows PC titles
}
let cachedHiddenIds: string[] | null = null;
let cachedIgnoredIds: string[] | null = null;

function getHiddenIds(): string[] {
  if (cachedHiddenIds !== null) return cachedHiddenIds as string[];
  try {
    const p = path.join(app.getPath('userData'), 'hidden_games.json');
    if (fs.existsSync(p)) {
      const parsed = JSON.parse(fs.readFileSync(p, 'utf-8'));
      cachedHiddenIds = isStringArray(parsed) ? parsed.map(validateGameId) : [];
    }
    else cachedHiddenIds = [];
  } catch(e) { cachedHiddenIds = []; }
  return cachedHiddenIds as string[];
}

function getIgnoredIds(): string[] {
  if (cachedIgnoredIds !== null) return cachedIgnoredIds as string[];
  try {
    const p = path.join(app.getPath('userData'), 'ignored_games.json');
    if (fs.existsSync(p)) {
      const parsed = JSON.parse(fs.readFileSync(p, 'utf-8'));
      cachedIgnoredIds = isStringArray(parsed) ? parsed.map(validateGameId) : [];
    }
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


function isStringArray(value: unknown): value is string[] {
  return Array.isArray(value) && value.every(item => typeof item === 'string');
}

function validatePersistedGame(value: unknown): GameEntry | null {
  if (!value || typeof value !== 'object') return null;
  const game = value as Partial<GameEntry>;
  try {
    const id = validateGameId(game.id);
    if (typeof game.name !== 'string' || game.name.length === 0 || game.name.length > 256) return null;
    const executablePath = canonicalExistingPath(game.executablePath || '', 'file');
    if (path.extname(executablePath).toLowerCase() !== '.exe') return null;
    const installPath = canonicalExistingPath(path.dirname(executablePath), 'directory');
    return {
      id,
      name: game.name,
      installPath,
      executablePath,
      sizeGB: Number.isFinite(game.sizeGB) ? Number(game.sizeGB) : 0,
      api: game.api === 'DX12' || game.api === 'Vulkan' ? game.api : 'DX11',
      compat: game.compat || 'unknown',
      hasInjector: fs.existsSync(resolveWithinRoot(installPath, 'vrinject.dll')),
    };
  } catch {
    return null;
  }
}

ipcMain.handle('library:scan', async (event): Promise<{ active: GameEntry[], waiting: GameEntry[] }> => {
  assertTrustedIpcSender(event);
  const games: GameEntry[] = [];
  const waitingGames: GameEntry[] = [];
  const seenIds = new Set<string>();

  let hiddenIds = getHiddenIds();
  let ignoredIds = getIgnoredIds();
  let compatList: Record<string, any> = {};

  const defaultCompatList: Record<string, string> = {
    '990080': 'verified',
    'fa4240e57a3c46b39f169041b7811293': 'verified',
    '864c7bc2c2394f7dbd1b534aa068ff56': 'verified',
    '1091500': 'verified',
    '1245620': 'verified',
    '814380': 'verified',
    '668580': 'verified',
    '1110910': 'verified',
    '1623730': 'verified',
  };

  function scanLauncherGameArt(launcher: 'epic' | 'steam', gameId: string, displayName: string, canonicalSteamPath?: string): string | undefined {
    if (launcher === 'epic') {
      try {
        const lower = (displayName || '').toLowerCase().trim();
        const candidateDirs = [
          path.join(__dirname, '..', 'public'),
          path.join(__dirname, '..', 'frontend-dist'),
          path.join(process.cwd(), 'launcher', 'public'),
          path.join(process.cwd(), 'launcher', 'frontend-dist'),
          path.join(process.cwd(), 'public'),
        ];
        
        let filename = '';
        if (lower.includes('hogwarts') || gameId === 'fa4240e57a3c46b39f169041b7811293' || gameId === '864c7bc2c2394f7dbd1b534aa068ff56') {
          filename = 'epic_hogwarts_legacy.jpg';
        }

        if (filename) {
          for (const d of candidateDirs) {
            const p = path.join(d, filename);
            if (fs.existsSync(p)) {
              try {
                const buf = fs.readFileSync(p);
                return `data:image/jpeg;base64,${buf.toString('base64')}`;
              } catch {}
            }
          }
        }

        // Scan Epic's catalog database (catcache.bin)
        const catcachePath = path.join(process.env.PROGRAMDATA || 'C:\\ProgramData', 'Epic', 'EpicGamesLauncher', 'Data', 'Catalog', 'catcache.bin');
        if (fs.existsSync(catcachePath)) {
          const b64 = fs.readFileSync(catcachePath, 'utf-8');
          const catalog = JSON.parse(Buffer.from(b64, 'base64').toString('utf-8'));
          if (Array.isArray(catalog)) {
            const item = catalog.find((x: any) => 
              x.id === gameId || 
              x.namespace === gameId || 
              x.entitlementName === gameId ||
              (x.title && x.title.toLowerCase().trim() === lower)
            );
            if (item && item.keyImages && Array.isArray(item.keyImages)) {
              const preferred = item.keyImages.find((k: any) => k.type === 'DieselGameBox') ||
                                item.keyImages.find((k: any) => k.type === 'Thumbnail') ||
                                item.keyImages[0];
              if (preferred && preferred.url && fs.existsSync(preferred.url)) {
                return `data:image/jpeg;base64,${fs.readFileSync(preferred.url).toString('base64')}`;
              }
            }
          }
        }
      } catch (e) {}
    } else if (launcher === 'steam' && canonicalSteamPath) {
      try {
        const cacheDir = path.join(canonicalSteamPath, 'appcache', 'librarycache', gameId);
        const steamCandidates = [
          path.join(cacheDir, 'icon.jpg'),
          path.join(cacheDir, 'logo.png'),
          path.join(cacheDir, 'header.jpg'),
          path.join(canonicalSteamPath, 'steam', 'games', `${gameId}.ico`),
        ];
        for (const cand of steamCandidates) {
          if (fs.existsSync(cand)) {
            try {
              const ext = cand.endsWith('.png') ? 'png' : cand.endsWith('.ico') ? 'x-icon' : 'jpeg';
              return `data:image/${ext};base64,${fs.readFileSync(cand).toString('base64')}`;
            } catch {}
          }
        }
      } catch (e) {}
    }
    return undefined;
  }

  try {
    const compatGamesFile = path.join(app.getPath('userData'), 'compat_games.json');
    if (fs.existsSync(compatGamesFile)) {
      const parsed = JSON.parse(fs.readFileSync(compatGamesFile, 'utf-8'));
      if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) compatList = parsed;
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
    const canonicalSteamPath = canonicalExistingPath(steamPath, 'directory');
    const vdfPath = resolveWithinRoot(canonicalSteamPath, 'steamapps', 'libraryfolders.vdf');
    
    if (!fs.existsSync(vdfPath)) return { active: games, waiting: waitingGames };
    const vdfContent = fs.readFileSync(vdfPath, 'utf-8');
    
    const paths = Array.from(vdfContent.matchAll(/"path"\s+"([^"]+)"/g)).map(m => m[1].replace(/\\\\/g, '\\'));
    if (!paths.includes(canonicalSteamPath)) paths.push(canonicalSteamPath);
    
    for (const libPath of paths) {
      let canonicalLibrary: string;
      try {
        canonicalLibrary = canonicalExistingPath(libPath, 'directory');
      } catch {
        continue;
      }
      const appsDir = resolveWithinRoot(canonicalLibrary, 'steamapps');
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
          
          const installPath = resolveWithinRoot(appsDir, 'common', dirStr);
          if (!fs.existsSync(installPath)) continue;
          
          let api = detectAPI(installPath);
          if (nameStr.toLowerCase().includes('hogwarts') || idStr === '990080') {
            api = 'DX12';
          }
          if (api === 'Unknown') api = 'DX11';
          
          const ac = detectAntiCheat(installPath);
          const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
          
          if (ignoredIds.includes(idStr)) continue;
          
          let iconBase64: string | undefined = scanLauncherGameArt('steam', idStr, nameStr, canonicalSteamPath);
          const primaryExe = findPrimaryExecutable(installPath);
          if (!iconBase64 && primaryExe && fs.existsSync(primaryExe)) {
            try {
              const icon = await app.getFileIcon(primaryExe, { size: 'large' });
              iconBase64 = icon.toDataURL();
            } catch {}
          }
          if (primaryExe) gameExeMap[idStr] = primaryExe;

          const compatStatus = compatList[idStr] || defaultCompatList[idStr] || (nameStr.toLowerCase().includes('hogwarts') ? 'verified' : 'unknown');

          const entry: GameEntry = {
            id: idStr,
            name: nameStr,
            installPath,
            executablePath: primaryExe || '',
            sizeGB: 0,
            api,
            compat: compatStatus as any,
            hasInjector,
            hasAntiCheat: ac.hasAntiCheat,
            antiCheatName: ac.antiCheatName,
            iconBase64
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
      const manifestsPath = path.join(process.env.PROGRAMDATA || 'C:\\ProgramData', 'Epic', 'EpicGamesLauncher', 'Data', 'Manifests');
      if (fs.existsSync(manifestsPath)) {
        const files = fs.readdirSync(manifestsPath);
        for (const file of files) {
          if (!file.endsWith('.item')) continue;
          try {
            const data = fs.readFileSync(path.join(manifestsPath, file), 'utf-8');
            const parsed = JSON.parse(data);
            const rawInstallPath = parsed.InstallLocation;
            if (typeof rawInstallPath === 'string' && fs.existsSync(rawInstallPath)) {
                const installPath = canonicalExistingPath(rawInstallPath, 'directory');
                let api = detectAPI(installPath);
                
                const ac = detectAntiCheat(installPath);
                const id = validateGameId(parsed.AppName || parsed.CatalogItemId);
                const dName = typeof parsed.DisplayName === 'string' ? parsed.DisplayName : id;
                if (isIgnoredSoftware(dName)) continue;
                if (ignoredIds.includes(id)) continue;
                
                let iconBase64: string | undefined = scanLauncherGameArt('epic', id, dName);
                const exeName = parsed.LaunchExecutable || parsed.Executable;
                let epicExePath = '';
                const primaryExe = findPrimaryExecutable(installPath);
                if (primaryExe && fs.existsSync(primaryExe)) {
                  epicExePath = primaryExe;
                } else if (exeName) {
                  const directPath = resolveWithinRoot(installPath, exeName);
                  if (fs.existsSync(directPath)) {
                    epicExePath = directPath;
                  }
                }
                if (epicExePath && fs.existsSync(epicExePath)) {
                  if (!iconBase64) {
                    try {
                      const icon = await app.getFileIcon(epicExePath, { size: 'large' });
                      iconBase64 = icon.toDataURL();
                    } catch(e) {}
                  }
                  gameExeMap[id] = epicExePath;
                }

                if (dName.toLowerCase().includes('hogwarts') || id === 'fa4240e57a3c46b39f169041b7811293' || id === '864c7bc2c2394f7dbd1b534aa068ff56') {
                  api = 'DX12';
                }
                if (api === 'Unknown') api = 'DX11';

                let hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
                if (!hasInjector && epicExePath) {
                  hasInjector = fs.existsSync(path.join(path.dirname(epicExePath), 'vrinject.dll'));
                }

                const compatStatus = compatList[id] || defaultCompatList[id] || (dName.toLowerCase().includes('hogwarts') ? 'verified' : 'unknown');

                if (!seenIds.has(id)) {
                  seenIds.add(id);
                  const entry: GameEntry = {
                    id: id,
                    name: parsed.DisplayName,
                    installPath,
                    executablePath: epicExePath,
                    sizeGB: 0,
                    api,
                    compat: compatStatus as any,
                    hasInjector,
                    hasAntiCheat: ac.hasAntiCheat,
                    antiCheatName: ac.antiCheatName,
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
        const rawCustomGames: unknown = JSON.parse(customGamesData);
        if (!Array.isArray(rawCustomGames)) throw new Error('Invalid custom game list');
        for (const rawGame of rawCustomGames) {
          const cg = validatePersistedGame(rawGame);
          if (!cg) continue;
          if (!seenIds.has(cg.id)) {
            if (ignoredIds.includes(cg.id)) continue;
            seenIds.add(cg.id);
            if (cg.executablePath && fs.existsSync(cg.executablePath)) {
              const detected = inspectExeAPI(cg.executablePath);
              if (detected !== 'Unknown') {
                cg.api = detected;
              }
            }
            if (cg.name.toLowerCase().includes('penguinhotel') && cg.installPath.toUpperCase().includes('MECCHA CHAMELEON')) {
              cg.name = 'MECCHA CHAMELEON';
            }
            const launcherArt = scanLauncherGameArt('epic', cg.id, cg.name) || scanLauncherGameArt('steam', cg.id, cg.name);
            if (launcherArt) {
              cg.iconBase64 = launcherArt;
            } else {
              try {
                 if (fs.existsSync(cg.executablePath)) {
                    const icon = await app.getFileIcon(cg.executablePath, { size: 'large' });
                    cg.iconBase64 = icon.toDataURL();
                 }
              } catch(e) {}
            }
            const ac = detectAntiCheat(cg.installPath);
            cg.hasAntiCheat = ac.hasAntiCheat;
            cg.antiCheatName = ac.antiCheatName;
            if (cg.executablePath) {
              cg.hasInjector = fs.existsSync(path.join(path.dirname(cg.executablePath), 'vrinject.dll'));
            }
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

ipcMain.handle('library:addCustom', async (event): Promise<{ success: boolean }> => {
  assertTrustedIpcSender(event);
  const { canceled, filePaths } = await dialog.showOpenDialog({
    properties: ['openFile'],
    filters: [{ name: 'Executables', extensions: ['exe'] }]
  });
  
  if (canceled || filePaths.length === 0) return { success: false };
  
  const exePath = canonicalExistingPath(filePaths[0], 'file');
  const installPath = path.dirname(exePath);
  const name = path.basename(exePath, '.exe');
  const id = 'custom_' + Date.now().toString();
  
  let api: 'DX11' | 'DX12' | 'Vulkan' | 'Unknown' = 'DX11';
  try {
     const detected = inspectExeAPI(exePath);
     if (detected !== 'Unknown') {
       api = detected;
     } else {
       const filesInInstall = fs.readdirSync(installPath);
       const hasVulkan = filesInInstall.some(f => f.toLowerCase() === 'vulkan-1.dll' || f.toLowerCase().endsWith('.spv'));
       const hasDX12 = filesInInstall.some(f => f.toLowerCase() === 'd3d12.dll' || f.toLowerCase() === 'd3d12core.dll');
       if (hasVulkan) api = 'Vulkan';
       else if (hasDX12) api = 'DX12';
     }
  } catch(e) {}
  
  const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
  
  let iconBase64 = undefined;
  try {
    const icon = await app.getFileIcon(exePath, { size: 'large' });
    iconBase64 = icon.toDataURL();
  } catch(e) {}
  
  const isSekiro = name.toLowerCase().includes('sekiro') || exePath.toLowerCase().includes('sekiro');
  const newGame: GameEntry = {
    id,
    name,
    installPath,
    executablePath: exePath,
    sizeGB: 0,
    api: isSekiro ? 'DX11' : api,
    compat: isSekiro ? 'verified' : 'unknown',
    hasInjector,
    iconBase64
  };
  
  try {
    gameExeMap[id] = exePath;
    const customGamesFile = path.join(app.getPath('userData'), 'custom_games.json');
    let existing: GameEntry[] = [];
    if (fs.existsSync(customGamesFile)) {
      const parsed = JSON.parse(fs.readFileSync(customGamesFile, 'utf-8'));
      if (Array.isArray(parsed)) existing = parsed.map(validatePersistedGame).filter((x): x is GameEntry => x !== null);
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
    assertTrustedIpcSender(event);
    id = validateGameId(id);
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
    assertTrustedIpcSender(event);
    id = validateGameId(id);
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
    assertTrustedIpcSender(event);
    id = validateGameId(id);
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

ipcMain.handle('library:restoreIgnoredGames', async (event): Promise<{ success: boolean }> => {
  try {
    assertTrustedIpcSender(event);
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
