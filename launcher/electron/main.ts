import { app, BrowserWindow, ipcMain, protocol, dialog, net } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as crypto from 'crypto';
import { pathToFileURL } from 'url';
import { assertTrustedIpcSender, resolveWithinRoot } from './utils';

const isDev = !app.isPackaged;
if (isDev) {
  // Isolate development session data from installed/packaged app to prevent singleton lock conflicts (Error code: 32)
  app.setPath('userData', path.join(app.getPath('appData'), 'NexVR-Dev'));
} else {
  // Production gets its own clean, dedicated userData directory
  app.setPath('userData', path.join(app.getPath('appData'), 'NexVR Engine'));
}

// Enforce single instance lock to prevent cache collisions and multiple windows
const gotTheLock = app.requestSingleInstanceLock();
if (!gotTheLock) {
  app.quit();
  process.exit(0);
} else {
  app.on('second-instance', () => {
    if (mainWindow && !mainWindow.isDestroyed()) {
      if (mainWindow.isMinimized()) mainWindow.restore();
      mainWindow.focus();
    }
  });
}

// Disable GPU disk cache in dev mode to prevent Chromium Windows file lock errors (0x5)
app.commandLine.appendSwitch('disable-gpu-shader-disk-cache');
// Disable Chromium OS-level process sandbox on Windows to prevent silent startup crashes
// when installed in user profile directories (e.g. %LOCALAPPDATA%\Programs) with restricted ACLs.
app.commandLine.appendSwitch('no-sandbox');

// Initialize core authorization token for DLL injector validation
const NEXVR_AUTH_TOKEN = crypto.randomUUID();
process.env.NEXVR_AUTH_TOKEN = NEXVR_AUTH_TOKEN;

// Register the custom app scheme for sandboxed context assets loading
protocol.registerSchemesAsPrivileged([{
  scheme: 'nexvr',
  privileges: {
    standard: true,
    secure: true,
    supportFetchAPI: true,
    corsEnabled: true,
    bypassCSP: false,
  }
}]);

process.on('uncaughtException', (err) => {
  dialog.showErrorBox('Uncaught Exception', err.stack || err.message || String(err));
});
process.on('unhandledRejection', (reason: any) => {
  const msg = reason?.message || String(reason || '');
  if (msg.includes('Object has been destroyed')) {
    return; // Ignore window teardown race conditions
  }
  dialog.showErrorBox('Unhandled Rejection', reason?.stack || reason?.message || String(reason));
});

let mainWindow: BrowserWindow | null = null;

const MIME_TYPES: Record<string, string> = {
  '.html': 'text/html',
  '.js':   'text/javascript',
  '.css':  'text/css',
  '.json': 'application/json',
  '.png':  'image/png',
  '.jpg':  'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.gif':  'image/gif',
  '.svg':  'image/svg+xml',
  '.ico':  'image/x-icon',
  '.woff': 'font/woff',
  '.woff2':'font/woff2',
  '.ttf':  'font/ttf',
  '.eot':  'application/vnd.ms-fontobject',
  '.map':  'application/json',
};

function createWindow() {
  const iconCandidate = path.join(__dirname, '../../assets/icon.ico');
  const windowIcon = fs.existsSync(iconCandidate) ? iconCandidate : undefined;

  mainWindow = new BrowserWindow({
    width: 1000,
    height: 700,
    minWidth: 900,
    minHeight: 650,
    center: true,
    show: true,
    backgroundColor: '#0a0d14',
    frame: false,
    icon: windowIcon,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: false,
      webSecurity: true,
      allowRunningInsecureContent: false,
      preload: path.join(__dirname, 'preload.js'),
    },
  });

  mainWindow.webContents.on('render-process-gone', (_event, details) => {
    console.error('[CRITICAL] Render process gone:', JSON.stringify(details));
  });

  mainWindow.center();
  mainWindow.focus();

  if (isDev && process.env.VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL).catch(err => {
      console.error('loadURL Error:', err);
    });
    mainWindow.webContents.openDevTools();
  } else {
    const indexPath = path.join(__dirname, '..', '..', 'frontend-dist', 'index.html');
    mainWindow.loadURL('nexvr://app/index.html').catch(err => {
      console.warn('[LOAD] Custom protocol load failed, falling back to direct loadFile:', err);
      if (mainWindow && !mainWindow.isDestroyed()) {
        mainWindow.loadFile(indexPath).catch(fileErr => {
          if (mainWindow && !mainWindow.isDestroyed()) {
            dialog.showErrorBox('loadURL Error', String(fileErr));
          }
        });
      }
    });
  }

  mainWindow.webContents.on('will-navigate', (event, url) => {
    if (!isDev && !url.startsWith('nexvr://app/') && !url.startsWith('file://')) {
      event.preventDefault();
      console.log(`[SECURITY] Blocked navigation to: ${url}`);
    }
  });

  mainWindow.webContents.setWindowOpenHandler(() => {
    return { action: 'deny' };
  });
}

app.whenReady().then(() => {
  const frontendDir = path.join(__dirname, '..', '..', 'frontend-dist');

  protocol.handle('nexvr', async (request) => {
    const url = new URL(request.url);
    if (url.hostname !== 'app' || request.method !== 'GET') {
      return new Response('Forbidden', { status: 403 });
    }
    let filePath = decodeURIComponent(url.pathname);
    if (filePath.startsWith('/')) filePath = filePath.substring(1);
    if (!filePath || filePath === '' || filePath === '/') filePath = 'index.html';

    try {
      const fullPath = resolveWithinRoot(frontendDir, filePath);
      try {
        const fileUrl = pathToFileURL(fullPath).toString();
        return await net.fetch(fileUrl);
      } catch {
        const ext = path.extname(filePath).toLowerCase();
        const mimeType = MIME_TYPES[ext] || 'application/octet-stream';
        const buffer = fs.readFileSync(fullPath);
        return new Response(buffer, {
          status: 200,
          headers: { 'Content-Type': mimeType }
        });
      }
    } catch (err: any) {
      console.error('[nexvr://] Failed to serve requested asset:', err.message);
      return new Response('Not Found', { status: 404, headers: { 'Content-Type': 'text/plain' } });
    }
  });

  createWindow();

  // Background check for instant engine hotfix on startup
  setTimeout(() => {
    checkForEngineHotfix().catch(() => {});
  }, 2000);

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

// Import sub-modules to register their IPC handlers
import './libraryManager';
import './configManager';
import './injectionManager';
import './diagnosticsManager';
import { checkForEngineHotfix } from './updateManager';

// Native Window Controls Handler
ipcMain.on('window:minimize', (event) => {
  assertTrustedIpcSender(event);
  if (mainWindow) mainWindow.minimize();
});

ipcMain.on('window:maximize', (event) => {
  assertTrustedIpcSender(event);
  if (mainWindow) {
    if (mainWindow.isMaximized()) mainWindow.unmaximize();
    else mainWindow.maximize();
  }
});

ipcMain.on('window:close', (event) => {
  assertTrustedIpcSender(event);
  if (mainWindow) mainWindow.close();
});
