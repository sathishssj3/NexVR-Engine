import { contextBridge, ipcRenderer } from 'electron';

contextBridge.exposeInMainWorld('ag', {
  library: {
    scan: () => ipcRenderer.invoke('library:scan'),
    addCustom: () => ipcRenderer.invoke('library:addCustom'),
    removeGame: (id: string) => ipcRenderer.invoke('library:removeGame', id),
    restoreGame: (id: string) => ipcRenderer.invoke('library:restoreGame', id),
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
  utils: {
    openConfig: (id: string) => ipcRenderer.invoke('utils:openConfig', id),
    openLog: (id: string) => ipcRenderer.invoke('utils:openLog', id),
  },
  inject: {
    deploy: (id: string) =>
              ipcRenderer.invoke('inject:deploy', id),
    cancel: () =>
              ipcRenderer.invoke('inject:cancel'),
    monitor: (pid: number) =>
              ipcRenderer.invoke('inject:monitor', pid),
  },
  log: {
    onLine: (cb: (line: string) => void) => {
      ipcRenderer.on('log:line', (_, l) => {
        if (typeof l === 'string') cb(l);
      });
    },
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
