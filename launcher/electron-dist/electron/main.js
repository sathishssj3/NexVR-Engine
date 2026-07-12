"use strict";
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
const crypto = __importStar(require("crypto"));
const utils_1 = require("./utils");
// Initialize core authorization token for DLL injector validation
const NEXVR_AUTH_TOKEN = crypto.randomUUID();
process.env.NEXVR_AUTH_TOKEN = NEXVR_AUTH_TOKEN;
const isDev = !electron_1.app.isPackaged;
// Register the custom app scheme for sandboxed context assets loading
electron_1.protocol.registerSchemesAsPrivileged([{
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
    electron_1.dialog.showErrorBox('Uncaught Exception', err.stack || err.message || String(err));
});
process.on('unhandledRejection', (reason) => {
    electron_1.dialog.showErrorBox('Unhandled Rejection', reason?.stack || reason?.message || String(reason));
});
let mainWindow = null;
const MIME_TYPES = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.css': 'text/css',
    '.json': 'application/json',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.jpeg': 'image/jpeg',
    '.gif': 'image/gif',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon',
    '.woff': 'font/woff',
    '.woff2': 'font/woff2',
    '.ttf': 'font/ttf',
    '.eot': 'application/vnd.ms-fontobject',
    '.map': 'application/json',
};
function createWindow() {
    mainWindow = new electron_1.BrowserWindow({
        width: 1000,
        height: 700,
        minWidth: 900,
        minHeight: 650,
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
    if (isDev && process.env.VITE_DEV_SERVER_URL) {
        mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL).catch(err => {
            console.error('loadURL Error:', err);
        });
        mainWindow.webContents.openDevTools();
    }
    else {
        mainWindow.loadURL('nexvr://app/index.html').catch(err => {
            electron_1.dialog.showErrorBox('loadURL Error', String(err));
        });
    }
    mainWindow.webContents.on('will-navigate', (event, url) => {
        if (!isDev && !url.startsWith('nexvr://app/')) {
            event.preventDefault();
            console.log(`[SECURITY] Blocked navigation to: ${url}`);
        }
    });
    mainWindow.webContents.setWindowOpenHandler(() => {
        return { action: 'deny' };
    });
}
electron_1.app.whenReady().then(() => {
    const frontendDir = path.join(__dirname, '..', '..', 'frontend-dist');
    electron_1.protocol.handle('nexvr', (request) => {
        const url = new URL(request.url);
        if (url.hostname !== 'app' || request.method !== 'GET') {
            return new Response('Forbidden', { status: 403 });
        }
        let filePath = decodeURIComponent(url.pathname);
        if (filePath.startsWith('/'))
            filePath = filePath.substring(1);
        if (!filePath || filePath === '' || filePath === '/')
            filePath = 'index.html';
        try {
            const fullPath = (0, utils_1.resolveWithinRoot)(frontendDir, filePath);
            const nodeBuffer = fs.readFileSync(fullPath);
            const uint8 = new Uint8Array(nodeBuffer.buffer, nodeBuffer.byteOffset, nodeBuffer.byteLength);
            const ext = path.extname(filePath).toLowerCase();
            const mimeType = MIME_TYPES[ext] || 'application/octet-stream';
            return new Response(uint8, {
                status: 200,
                headers: { 'Content-Type': mimeType },
            });
        }
        catch (err) {
            console.error('[nexvr://] Failed to serve requested asset', err.message);
            return new Response('Not Found', { status: 404, headers: { 'Content-Type': 'text/plain' } });
        }
    });
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
// Import sub-modules to register their IPC handlers
require("./libraryManager");
require("./configManager");
require("./injectionManager");
require("./diagnosticsManager");
// Native Window Controls Handler
electron_1.ipcMain.on('window:minimize', (event) => {
    (0, utils_1.assertTrustedIpcSender)(event);
    if (mainWindow)
        mainWindow.minimize();
});
electron_1.ipcMain.on('window:maximize', (event) => {
    (0, utils_1.assertTrustedIpcSender)(event);
    if (mainWindow) {
        if (mainWindow.isMaximized())
            mainWindow.unmaximize();
        else
            mainWindow.maximize();
    }
});
electron_1.ipcMain.on('window:close', (event) => {
    (0, utils_1.assertTrustedIpcSender)(event);
    if (mainWindow)
        mainWindow.close();
});
