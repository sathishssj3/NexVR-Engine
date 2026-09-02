import { app, BrowserWindow, ipcMain, protocol, dialog, net } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as crypto from 'crypto';
import { pathToFileURL } from 'url';
import { assertTrustedIpcSender, resolveWithinRoot } from './utils';

// Enforce single instance lock to prevent cache collisions and multiple windows
const gotTheLock = app.requestSingleInstanceLock();
if (!gotTheLock) {
  app.quit();
} else {
  app.on('second-instance', () => {
    if (mainWindow) {
      if (mainWindow.isMinimized()) mainWindow.restore();
      mainWindow.focus();
    }
  });
}

// Disable GPU disk cache in dev mode to prevent Chromium Windows file lock errors (0x5)
app.commandLine.appendSwitch('disable-gpu-shader-disk-cache');

// Initialize core authorization token for DLL injector validation
const NEXVR_AUTH_TOKEN = crypto.randomUUID();
process.env.NEXVR_AUTH_TOKEN = NEXVR_AUTH_TOKEN;

const isDev = !app.isPackaged;

// Register the custom app scheme for sandboxed context assets loading
protocol.registerSchemesAsPrivileged([{
  scheme: 'nexvr',
  privileges: {
    standard: true,
    secure: true,
    supportFetchAPI: true,
    corsEnabled: false,
    bypassCSP: false,
  }
}]);

process.on('uncaughtException', (err) => {
  dialog.showErrorBox('Uncaught Exception', err.stack || err.message || String(err));
});
process.on('unhandledRejection', (reason: any) => {
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
  mainWindow = new BrowserWindow({
    width: 1000,
    height: 700,
    minWidth: 900,
    minHeight: 650,
    center: true,
    show: true,
    backgroundColor: '#0a0d14',
    frame: false,
    icon: path.join(__dirname, '../../assets/icon.ico'),
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      sandbox: true,
      webSecurity: true,
      allowRunningInsecureContent: false,
      preload: path.join(__dirname, 'preload.js'),
    },
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
      mainWindow?.loadFile(indexPath).catch(fileErr => {
        dialog.showErrorBox('loadURL Error', String(fileErr));
      });
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

  protocol.handle('nexvr', (request) => {
    const url = new URL(request.url);
    if (url.hostname !== 'app' || request.method !== 'GET') {
      return new Response('Forbidden', { status: 403 });
    }
    let filePath = decodeURIComponent(url.pathname);
    if (filePath.startsWith('/')) filePath = filePath.substring(1);
    if (!filePath || filePath === '' || filePath === '/') filePath = 'index.html';

    try {
      const fullPath = resolveWithinRoot(frontendDir, filePath);
      const fileUrl = pathToFileURL(fullPath).toString();
      return net.fetch(fileUrl);
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
