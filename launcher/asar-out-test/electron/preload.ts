import { contextBridge, ipcRenderer } from 'electron';

contextBridge.exposeInMainWorld('ag', {
  library: {
    scan: () => ipcRenderer.invoke('library:scan'),
    addCustom: () => ipcRenderer.invoke('library:addCustom'),
    removeGame: (id: string) => ipcRenderer.invoke('library:removeGame', id),
    ignoreGame: (id: string) => ipcRenderer.invoke('library:ignoreGame', id),
    restoreIgnoredGames: () => ipcRenderer.invoke('library:restoreIgnoredGames'),
  },
  vr: {
    status: () => ipcRenderer.invoke('vr:status'),
  },
  config: {
    read:  (id: string) =>
             ipcRenderer.invoke('config:read', id),
    write: (id: string, cfg: unknown) =>
             ipcRenderer.invoke('config:write', id, cfg),
  },
  inject: {
    deploy: (id: string) =>
              ipcRenderer.invoke('inject:deploy', id),
    cancel: () =>
              ipcRenderer.invoke('inject:cancel'),
  },
  log: {
    onLine: (cb: (line: string) => void) =>
              ipcRenderer.on('log:line', (_, l) => cb(l)),
    offLine: () =>
              ipcRenderer.removeAllListeners('log:line'),
    export: (lines: unknown) => ipcRenderer.invoke('log:export', lines),
  },
  window: {
    minimize: () => ipcRenderer.send('window:minimize'),
    maximize: () => ipcRenderer.send('window:maximize'),
    close: () => ipcRenderer.send('window:close'),
  },
  shell: {
    openExternal: (url: string) => ipcRenderer.invoke('shell:openExternal', url),
  },
  versions: {
    electron: process.versions.electron,
    node: process.versions.node,
    chrome: process.versions.chrome,
  }
});
