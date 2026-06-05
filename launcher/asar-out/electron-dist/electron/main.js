"use strict";
console.log('HELLO FIRST LINE');
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
const electron_1 = require("electron");
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
const child_process = __importStar(require("child_process"));
const util = __importStar(require("util"));
const execAsync = util.promisify(child_process.exec);
process.env['ELECTRON_DISABLE_SECURITY_WARNINGS'] = 'true';
const isDev = process.env.NODE_ENV === 'development';
fs.writeFileSync(path.join(electron_1.app.getPath('desktop'), 'ag_debug.txt'), 'Starting Antigravity...\n');
process.on('uncaughtException', (err) => {
    fs.appendFileSync(path.join(electron_1.app.getPath('desktop'), 'ag_debug.txt'), 'Uncaught Exception: ' + err.stack + '\n');
});
process.on('unhandledRejection', (reason) => {
    fs.appendFileSync(path.join(electron_1.app.getPath('desktop'), 'ag_debug.txt'), 'Unhandled Rejection: ' + reason + '\n');
});
let mainWindow = null;
function createWindow() {
    mainWindow = new electron_1.BrowserWindow({
        width: 1000,
        height: 700,
        minWidth: 900,
        minHeight: 650,
        frame: false,
        webPreferences: {
            nodeIntegration: false,
            contextIsolation: true,
            sandbox: true,
            preload: path.join(__dirname, 'preload.js'),
        },
    });
    if (isDev) {
        mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL || 'http://localhost:5173');
        mainWindow.webContents.openDevTools();
    }
    else {
        mainWindow.loadFile(path.join(__dirname, '../../dist/index.html'));
    }
}
electron_1.app.whenReady().then(() => {
    createWindow();
    electron_1.app.on('activate', () => {
        if (electron_1.BrowserWindow.getAllWindows().length === 0)
            createWindow();
    });
});
electron_1.app.on('window-all-closed', () => {
    if (process.platform !== 'darwin')
        electron_1.app.quit();
});
// IPC Handlers
const gameExeMap = {};
const isIgnoredSoftware = (name) => {
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
electron_1.ipcMain.handle('library:scan', async () => {
    const games = [];
    const waitingGames = [];
    const seenIds = new Set();
    let hiddenIds = [];
    let ignoredIds = [];
    let compatList = {};
    try {
        const hiddenGamesFile = path.join(electron_1.app.getPath('userData'), 'hidden_games.json');
        if (fs.existsSync(hiddenGamesFile)) {
            hiddenIds = JSON.parse(fs.readFileSync(hiddenGamesFile, 'utf-8'));
        }
        const ignoredGamesFile = path.join(electron_1.app.getPath('userData'), 'ignored_games.json');
        if (fs.existsSync(ignoredGamesFile)) {
            ignoredIds = JSON.parse(fs.readFileSync(ignoredGamesFile, 'utf-8'));
        }
        const compatGamesFile = path.join(electron_1.app.getPath('userData'), 'compat_games.json');
        if (fs.existsSync(compatGamesFile)) {
            compatList = JSON.parse(fs.readFileSync(compatGamesFile, 'utf-8'));
        }
    }
    catch (e) { }
    try {
        const regQuery = `reg query "HKCU\\Software\\Valve\\Steam" /v "SteamPath"`;
        const regOut = child_process.execSync(regQuery, { encoding: 'utf-8' });
        const match = regOut.match(/SteamPath\s+REG_SZ\s+(.+)/i);
        if (!match)
            return { active: games, waiting: waitingGames };
        const steamPath = match[1].trim().replace(/\//g, '\\');
        const vdfPath = path.join(steamPath, 'steamapps', 'libraryfolders.vdf');
        if (!fs.existsSync(vdfPath))
            return { active: games, waiting: waitingGames };
        const vdfContent = fs.readFileSync(vdfPath, 'utf-8');
        const paths = Array.from(vdfContent.matchAll(/"path"\s+"([^"]+)"/g)).map(m => m[1].replace(/\\\\/g, '\\'));
        if (!paths.includes(steamPath))
            paths.push(steamPath);
        const seenIds = new Set();
        for (const libPath of paths) {
            const appsDir = path.join(libPath, 'steamapps');
            if (!fs.existsSync(appsDir))
                continue;
            const files = fs.readdirSync(appsDir);
            for (const file of files) {
                if (!file.startsWith('appmanifest_') || !file.endsWith('.acf'))
                    continue;
                try {
                    const content = fs.readFileSync(path.join(appsDir, file), 'utf-8');
                    const idMatch = content.match(/"appid"\s+"([^"]+)"/);
                    const nameMatch = content.match(/"name"\s+"([^"]+)"/);
                    const dirMatch = content.match(/"installdir"\s+"([^"]+)"/);
                    if (!idMatch || !nameMatch || !dirMatch)
                        continue;
                    if (isIgnoredSoftware(nameMatch[1]))
                        continue;
                    if (seenIds.has(idMatch[1]))
                        continue;
                    seenIds.add(idMatch[1]);
                    const installPath = path.join(appsDir, 'common', dirMatch[1]);
                    if (!fs.existsSync(installPath))
                        continue;
                    let api = 'DX11';
                    const filesInInstall = fs.readdirSync(installPath);
                    const hasVulkan = filesInInstall.some(f => f.toLowerCase() === 'vulkan-1.dll' || f.endsWith('.spv'));
                    const hasDX12 = filesInInstall.some(f => f.toLowerCase() === 'd3d12.dll' || f.toLowerCase() === 'd3d12core.dll');
                    if (hasVulkan)
                        api = 'Vulkan';
                    else if (hasDX12)
                        api = 'DX12';
                    const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
                    if (ignoredIds.includes(idMatch[1]))
                        continue;
                    const entry = {
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
                    }
                    else {
                        games.push(entry);
                    }
                }
                catch (e) {
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
                    if (!file.endsWith('.item'))
                        continue;
                    try {
                        const data = fs.readFileSync(path.join(manifestsPath, file), 'utf-8');
                        const parsed = JSON.parse(data);
                        const installPath = parsed.InstallLocation;
                        if (installPath && fs.existsSync(installPath)) {
                            let api = 'DX11';
                            try {
                                const filesInInstall = fs.readdirSync(installPath);
                                const hasVulkan = filesInInstall.some(f => f.toLowerCase() === 'vulkan-1.dll' || f.toLowerCase().endsWith('.spv'));
                                const hasDX12 = filesInInstall.some(f => f.toLowerCase() === 'd3d12.dll' || f.toLowerCase() === 'd3d12core.dll');
                                if (hasVulkan)
                                    api = 'Vulkan';
                                else if (hasDX12)
                                    api = 'DX12';
                            }
                            catch (e) { }
                            const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
                            const id = parsed.AppName || parsed.CatalogItemId;
                            const dName = parsed.DisplayName;
                            if (isIgnoredSoftware(dName))
                                continue;
                            if (ignoredIds.includes(id))
                                continue;
                            let iconBase64 = undefined;
                            const exeName = parsed.Executable;
                            let epicExePath = '';
                            if (exeName) {
                                epicExePath = path.join(installPath, exeName);
                                if (fs.existsSync(epicExePath)) {
                                    try {
                                        const icon = await electron_1.app.getFileIcon(epicExePath, { size: 'large' });
                                        iconBase64 = icon.toDataURL();
                                    }
                                    catch (e) { }
                                }
                            }
                            if (!seenIds.has(id)) {
                                seenIds.add(id);
                                const entry = {
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
                                }
                                else {
                                    games.push(entry);
                                }
                            }
                        }
                    }
                    catch (e) { }
                }
            }
        }
        catch (e) { }
        // Custom Games Scan
        try {
            const customGamesFile = path.join(electron_1.app.getPath('userData'), 'custom_games.json');
            if (fs.existsSync(customGamesFile)) {
                const customGamesData = fs.readFileSync(customGamesFile, 'utf-8');
                const customGamesParsed = JSON.parse(customGamesData);
                for (const cg of customGamesParsed) {
                    if (!seenIds.has(cg.id)) {
                        if (ignoredIds.includes(cg.id))
                            continue;
                        seenIds.add(cg.id);
                        try {
                            if (fs.existsSync(cg.executablePath) && !cg.name.toLowerCase().includes('sekiro')) {
                                const icon = await electron_1.app.getFileIcon(cg.executablePath, { size: 'large' });
                                cg.iconBase64 = icon.toDataURL();
                            }
                        }
                        catch (e) { }
                        gamePathsMap[cg.id] = cg.installPath;
                        gameExeMap[cg.id] = cg.executablePath;
                        if (hiddenIds.includes(cg.id)) {
                            waitingGames.push(cg);
                        }
                        else {
                            games.push(cg);
                        }
                    }
                }
            }
        }
        catch (e) { }
    }
    catch (e) {
        console.error('library:scan error', e);
    }
    return { active: games, waiting: waitingGames };
});
electron_1.ipcMain.handle('library:addCustom', async () => {
    const { canceled, filePaths } = await electron_1.dialog.showOpenDialog({
        properties: ['openFile'],
        filters: [{ name: 'Executables', extensions: ['exe'] }]
    });
    if (canceled || filePaths.length === 0)
        return { success: false };
    const exePath = filePaths[0];
    const installPath = path.dirname(exePath);
    const name = path.basename(exePath, '.exe');
    const id = 'custom_' + Date.now().toString();
    let api = 'DX11';
    try {
        const filesInInstall = fs.readdirSync(installPath);
        const hasVulkan = filesInInstall.some(f => f.toLowerCase() === 'vulkan-1.dll' || f.toLowerCase().endsWith('.spv'));
        const hasDX12 = filesInInstall.some(f => f.toLowerCase() === 'd3d12.dll' || f.toLowerCase() === 'd3d12core.dll');
        if (hasVulkan)
            api = 'Vulkan';
        else if (hasDX12)
            api = 'DX12';
    }
    catch (e) { }
    const hasInjector = fs.existsSync(path.join(installPath, 'vrinject.dll'));
    let iconBase64 = undefined;
    try {
        if (!name.toLowerCase().includes('sekiro')) {
            const icon = await electron_1.app.getFileIcon(exePath, { size: 'large' });
            iconBase64 = icon.toDataURL();
        }
    }
    catch (e) { }
    const newGame = {
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
        const customGamesFile = path.join(electron_1.app.getPath('userData'), 'custom_games.json');
        let existing = [];
        if (fs.existsSync(customGamesFile)) {
            existing = JSON.parse(fs.readFileSync(customGamesFile, 'utf-8'));
        }
        existing.push(newGame);
        fs.writeFileSync(customGamesFile, JSON.stringify(existing, null, 2));
        return { success: true };
    }
    catch (e) {
        return { success: false };
    }
});
electron_1.ipcMain.handle('library:removeGame', async (event, id) => {
    try {
        const hiddenGamesFile = path.join(electron_1.app.getPath('userData'), 'hidden_games.json');
        let hiddenIds = [];
        if (fs.existsSync(hiddenGamesFile)) {
            hiddenIds = JSON.parse(fs.readFileSync(hiddenGamesFile, 'utf-8'));
        }
        if (!hiddenIds.includes(id)) {
            hiddenIds.push(id);
            fs.writeFileSync(hiddenGamesFile, JSON.stringify(hiddenIds, null, 2));
        }
        return { success: true };
    }
    catch (e) {
        return { success: false };
    }
});
electron_1.ipcMain.handle('library:restoreGame', async (event, id) => {
    try {
        const hiddenGamesFile = path.join(electron_1.app.getPath('userData'), 'hidden_games.json');
        if (fs.existsSync(hiddenGamesFile)) {
            let hiddenIds = JSON.parse(fs.readFileSync(hiddenGamesFile, 'utf-8'));
            hiddenIds = hiddenIds.filter(x => x !== id);
            fs.writeFileSync(hiddenGamesFile, JSON.stringify(hiddenIds, null, 2));
        }
        return { success: true };
    }
    catch (e) {
        return { success: false };
    }
});
electron_1.ipcMain.handle('library:ignoreGame', async (event, id) => {
    try {
        const hiddenGamesFile = path.join(electron_1.app.getPath('userData'), 'hidden_games.json');
        if (fs.existsSync(hiddenGamesFile)) {
            let hiddenIds = JSON.parse(fs.readFileSync(hiddenGamesFile, 'utf-8'));
            hiddenIds = hiddenIds.filter(x => x !== id);
            fs.writeFileSync(hiddenGamesFile, JSON.stringify(hiddenIds, null, 2));
        }
        const ignoredGamesFile = path.join(electron_1.app.getPath('userData'), 'ignored_games.json');
        let ignoredIds = [];
        if (fs.existsSync(ignoredGamesFile)) {
            ignoredIds = JSON.parse(fs.readFileSync(ignoredGamesFile, 'utf-8'));
        }
        if (!ignoredIds.includes(id)) {
            ignoredIds.push(id);
            fs.writeFileSync(ignoredGamesFile, JSON.stringify(ignoredIds, null, 2));
        }
        if (id.startsWith('custom_')) {
            const customGamesFile = path.join(electron_1.app.getPath('userData'), 'custom_games.json');
            if (fs.existsSync(customGamesFile)) {
                let existing = JSON.parse(fs.readFileSync(customGamesFile, 'utf-8'));
                existing = existing.filter(g => g.id !== id);
                fs.writeFileSync(customGamesFile, JSON.stringify(existing, null, 2));
            }
            delete gameExeMap[id];
            delete gamePathsMap[id];
        }
        return { success: true };
    }
    catch (e) {
        return { success: false };
    }
});
electron_1.ipcMain.handle('library:restoreIgnoredGames', async () => {
    try {
        const ignoredGamesFile = path.join(electron_1.app.getPath('userData'), 'ignored_games.json');
        const hiddenGamesFile = path.join(electron_1.app.getPath('userData'), 'hidden_games.json');
        if (fs.existsSync(ignoredGamesFile)) {
            const ignoredIds = JSON.parse(fs.readFileSync(ignoredGamesFile, 'utf-8'));
            if (ignoredIds.length > 0) {
                let hiddenIds = [];
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
    }
    catch (e) {
        return { success: false };
    }
});
electron_1.ipcMain.handle('vr:status', async () => {
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
                }
                else if (rtPath.includes('steamvr')) {
                    runtime = 'SteamVR';
                    headset = 'SteamVR HMD';
                }
                else if (rtPath.includes('wmr') || rtPath.includes('mixedreality')) {
                    runtime = 'WMR';
                    headset = 'WMR Headset';
                }
            }
        }
        catch (e) { }
    }
    catch (e) { }
    return { connected, runtime, headset, refreshRate: 90 };
});
const defaultVRConfig = {
    motionAimSensitivity: 1.0,
    useRecommendedResolution: true,
    srgbCorrection: true,
    depthSubmission: false,
    rawInputMode: true,
    autoInjectOnLaunch: true,
};
let gamePathsMap = {};
electron_1.ipcMain.handle('config:read', async (_, id) => {
    try {
        const installPath = gamePathsMap[id];
        if (!installPath)
            return defaultVRConfig;
        const cfgPath = path.join(installPath, 'vrinject.json');
        if (fs.existsSync(cfgPath)) {
            const content = fs.readFileSync(cfgPath, 'utf-8');
            const parsed = JSON.parse(content);
            return { ...defaultVRConfig, ...parsed };
        }
        return defaultVRConfig;
    }
    catch (e) {
        return defaultVRConfig;
    }
});
electron_1.ipcMain.handle('config:write', async (_, id, cfg) => {
    try {
        const installPath = gamePathsMap[id];
        if (!installPath)
            return { success: false, error: 'Game path not found' };
        const cfgPath = path.join(installPath, 'vrinject.json');
        fs.writeFileSync(cfgPath, JSON.stringify(cfg, null, 2));
        return { success: true };
    }
    catch (e) {
        return { success: false, error: e.message };
    }
});
let cancelInjectionFlag = false;
let activeTargetExeName = '';
let activeTargetPid = 0;
let activeGameId = '';
electron_1.ipcMain.handle('inject:cancel', async () => {
    cancelInjectionFlag = true;
    try {
        // 1. Graceful taskkill (EAC blocks /F TerminateProcess, but allows graceful window close)
        if (activeTargetPid > 0) {
            await execAsync(`taskkill /PID ${activeTargetPid}`).catch(() => { });
            setTimeout(() => execAsync(`taskkill /PID ${activeTargetPid} /F`).catch(() => { }), 2000);
        }
        else if (activeTargetExeName) {
            await execAsync(`taskkill /IM "${activeTargetExeName}"`).catch(() => { });
            setTimeout(() => execAsync(`taskkill /IM "${activeTargetExeName}" /F`).catch(() => { }), 2000);
        }
        // 2. Common bootstrappers (Graceful first)
        await execAsync(`taskkill /IM "FallGuys_client.exe"`).catch(() => { });
        await execAsync(`taskkill /IM "launcher.exe"`).catch(() => { });
        setTimeout(() => execAsync(`taskkill /IM "FallGuys_client.exe" /F`).catch(() => { }), 2000);
        setTimeout(() => execAsync(`taskkill /IM "launcher.exe" /F`).catch(() => { }), 2000);
        // 3. Fallback: Aggressive case-insensitive folder wipe
        const installPath = gamePathsMap[activeGameId];
        if (installPath) {
            const cleanPath = installPath.toLowerCase().replace(/'/g, "''");
            await execAsync(`powershell -NoProfile -Command "$ErrorActionPreference = 'SilentlyContinue'; Get-Process | Where-Object { $_.Path -ne $null -and $_.Path.ToLower().StartsWith('${cleanPath}') } | Stop-Process -Force"`, { timeout: 3000 }).catch(() => { });
        }
    }
    catch (e) { }
});
electron_1.ipcMain.handle('inject:deploy', async (event, id) => {
    try {
        const installPath = gamePathsMap[id];
        if (!installPath)
            return { success: false, message: 'Game path not found' };
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
                        if (l.trim())
                            event.sender.send('log:line', l.trim());
                    }
                    lastSize = curr.size;
                }
                catch (e) { }
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
        }
        else if (/^\d+$/.test(id)) {
            electron_1.shell.openExternal('steam://rungameid/' + id);
        }
        else {
            electron_1.shell.openExternal(`com.epicgames.launcher://apps/${id}?action=launch&silent=true`);
        }
        // 4. Pre-scan for expected executable if unknown
        let targetExeName = '';
        if (gameExeMap[id]) {
            targetExeName = path.basename(gameExeMap[id]).toLowerCase();
        }
        else {
            let largestSize = 0;
            const scan = (dir) => {
                try {
                    const files = fs.readdirSync(dir);
                    for (const f of files) {
                        const fullPath = path.join(dir, f);
                        try {
                            const stats = fs.statSync(fullPath);
                            if (stats.isDirectory()) {
                                scan(fullPath);
                            }
                            else if (f.toLowerCase().endsWith('.exe')) {
                                const lower = f.toLowerCase();
                                if (lower.includes('launcher') || lower.includes('crash') || lower.includes('reporter') || lower.includes('anticheat') || lower.includes('eosbootstrapper') || lower.includes('start_protected_game')) {
                                    continue;
                                }
                                if (stats.size > largestSize) {
                                    largestSize = stats.size;
                                    targetExeName = f.toLowerCase();
                                }
                            }
                        }
                        catch (e) { }
                    }
                }
                catch (e) { }
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
        while (attempts < 120) { // Up to 60 seconds
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
                    if (!line.trim())
                        continue;
                    const parts = line.split('","');
                    if (parts.length < 2)
                        continue;
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
            }
            catch (e) { }
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
            child_process.execFile(cliPath, ['--pid', String(targetPid), '--dll', dllTarget], { timeout: 10000, killSignal: 'SIGKILL' }, (err, stdout, stderr) => {
                if (isResolved)
                    return;
                isResolved = true;
                clearTimeout(fallbackTimeout);
                if (stdout.trim())
                    event.sender.send('log:line', '[Injector CLI] ' + stdout.trim());
                if (stderr.trim())
                    event.sender.send('log:line', '[Injector CLI Error] ' + stderr.trim());
                if (err) {
                    fs.unwatchFile(logPath);
                    resolve({ success: false, message: `CLI failed: ${err.message}` });
                }
                else {
                    // Leave watcher running to continue streaming real game logs
                    resolve({ success: true, message: 'Deployed successfully', pid: targetPid });
                }
            });
        });
    }
    catch (e) {
        return { success: false, message: e.message };
    }
});
electron_1.ipcMain.handle('log:export', async (_, lines) => {
    if (!Array.isArray(lines))
        throw new Error('Invalid log lines');
    if (lines.length > 10000)
        throw new Error('Log too large');
    const sanitize = (line) => {
        let s = String(line).substring(0, 500);
        s = s.replace(/[A-Za-z]:\\[^\s"']+\\([^\s"'\\]+)/g, '$1');
        s = s.replace(/\\\\[^\s"']+\\([^\s"'\\]+)/g, '$1');
        s = s.replace(/PID\s+\d+/gi, 'PID [REDACTED]');
        s = s.replace(/pid:\s*\d+/gi, 'pid: [REDACTED]');
        return s;
    };
    const sanitized = lines.map(sanitize);
    const header = [
        '=== ANTIGRAVITY VR — SESSION LOG EXPORT ===',
        `Exported: ${new Date().toISOString()}`,
        `Lines: ${sanitized.length}`,
        'Note: Paths and PIDs have been redacted for privacy.',
        '============================================',
        '',
    ].join('\n');
    const content = header + sanitized.join('\n');
    const desktopPath = path.join(electron_1.app.getPath('desktop'), `antigravity_log_${Date.now()}.txt`);
    await fs.promises.writeFile(desktopPath, content, 'utf-8');
    await fs.promises.chmod(desktopPath, 0o600);
    return { success: true, path: desktopPath };
});
electron_1.ipcMain.handle('shell:openExternal', async (_, url) => {
    await electron_1.shell.openExternal(url);
});
electron_1.ipcMain.on('window:minimize', () => {
    if (mainWindow)
        mainWindow.minimize();
});
electron_1.ipcMain.on('window:maximize', () => {
    if (mainWindow) {
        if (mainWindow.isMaximized())
            mainWindow.unmaximize();
        else
            mainWindow.maximize();
    }
});
electron_1.ipcMain.on('window:close', () => {
    if (mainWindow)
        mainWindow.close();
});
