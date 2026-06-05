import { contextBridge, ipcRenderer } from 'electron';

contextBridge.exposeInMainWorld('ag', {
  steam: {
    scan: () => ipcRenderer.invoke('steam:scan'),
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
  },
  log: {
    onLine: (cb: (line: string) => void) =>
              ipcRenderer.on('log:line', (_, l) => cb(l)),
    offLine: () =>
              ipcRenderer.removeAllListeners('log:line'),
  },
});
