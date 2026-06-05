"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const electron_1 = require("electron");
electron_1.contextBridge.exposeInMainWorld('ag', {
    library: {
        scan: () => electron_1.ipcRenderer.invoke('library:scan'),
        addCustom: () => electron_1.ipcRenderer.invoke('library:addCustom'),
        removeGame: (id) => electron_1.ipcRenderer.invoke('library:removeGame', id),
        restoreGame: (id) => electron_1.ipcRenderer.invoke('library:restoreGame', id),
        ignoreGame: (id) => electron_1.ipcRenderer.invoke('library:ignoreGame', id),
        restoreIgnoredGames: () => electron_1.ipcRenderer.invoke('library:restoreIgnoredGames'),
    },
    vr: {
        status: () => electron_1.ipcRenderer.invoke('vr:status'),
    },
    config: {
        read: (id) => electron_1.ipcRenderer.invoke('config:read', id),
        write: (id, cfg) => electron_1.ipcRenderer.invoke('config:write', id, cfg),
    },
    utils: {
        openConfig: (id) => electron_1.ipcRenderer.invoke('utils:openConfig', id),
        openLog: (id) => electron_1.ipcRenderer.invoke('utils:openLog', id),
    },
    inject: {
        deploy: (id) => electron_1.ipcRenderer.invoke('inject:deploy', id),
        cancel: () => electron_1.ipcRenderer.invoke('inject:cancel'),
        monitor: (pid) => electron_1.ipcRenderer.invoke('inject:monitor', pid),
    },
    log: {
        onLine: (cb) => electron_1.ipcRenderer.on('log:line', (_, l) => cb(l)),
        offLine: () => electron_1.ipcRenderer.removeAllListeners('log:line'),
        export: (lines) => electron_1.ipcRenderer.invoke('log:export', lines),
    },
    window: {
        minimize: () => electron_1.ipcRenderer.send('window:minimize'),
        maximize: () => electron_1.ipcRenderer.send('window:maximize'),
        close: () => electron_1.ipcRenderer.send('window:close'),
    },
    shell: {
        openExternal: (url) => electron_1.ipcRenderer.invoke('shell:openExternal', url),
    },
    versions: {
        electron: process.versions.electron,
        node: process.versions.node,
        chrome: process.versions.chrome,
    }
});
