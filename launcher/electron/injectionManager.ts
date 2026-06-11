import { app, shell, ipcMain } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import * as util from 'util';
import { InjectResult } from '../src/types';
import { gamePathsMap, gameExeMap, validateGameId, safeGamePath } from './utils';

const execAsync = util.promisify(child_process.exec);
const isDev = !app.isPackaged;

export let cancelInjectionFlag = false;
export let activeTargetExeName = '';
export let activeTargetPid = 0;
export let activeGameId = '';
const injectRateLimits: Record<string, number[]> = {};

ipcMain.handle('inject:cancel', async () => {
   cancelInjectionFlag = true;
   try {
      if (activeTargetPid > 0) {
         await execAsync(`taskkill /PID ${activeTargetPid}`, { timeout: 2000 }).catch(() => {});
         setTimeout(() => execAsync(`taskkill /PID ${activeTargetPid} /F`, { timeout: 2000 }).catch(() => {}), 2000);
      } else if (activeTargetExeName) {
         await execAsync(`taskkill /IM "${activeTargetExeName}"`, { timeout: 2000 }).catch(() => {});
         setTimeout(() => execAsync(`taskkill /IM "${activeTargetExeName}" /F`, { timeout: 2000 }).catch(() => {}), 2000);
      }
      
      await execAsync(`taskkill /IM "FallGuys_client.exe"`, { timeout: 2000 }).catch(() => {});
      await execAsync(`taskkill /IM "launcher.exe"`, { timeout: 2000 }).catch(() => {});
      setTimeout(() => execAsync(`taskkill /IM "FallGuys_client.exe" /F`, { timeout: 2000 }).catch(() => {}), 2000);
      setTimeout(() => execAsync(`taskkill /IM "launcher.exe" /F`, { timeout: 2000 }).catch(() => {}), 2000);
      
      const installPath = gamePathsMap[activeGameId];
      if (installPath) {
         const cleanPath = installPath.toLowerCase().replace(/'/g, "''");
         await execAsync(`powershell -NoProfile -Command "$ErrorActionPreference = 'SilentlyContinue'; Get-Process | Where-Object { $_.Path -ne $null -and $_.Path.ToLower().StartsWith('${cleanPath}') } | Stop-Process -Force"`, { timeout: 3000 }).catch(() => {});
      }
   } catch(e) {}
});

ipcMain.handle('inject:deploy', async (event, id: string): Promise<InjectResult> => {
  try {
    const now = Date.now();
    const windowMs = 15 * 60 * 1000;
    if (!injectRateLimits[id]) injectRateLimits[id] = [];
    injectRateLimits[id] = injectRateLimits[id].filter(t => now - t < windowMs);
    if (injectRateLimits[id].length >= 1000) {
      return { success: false, message: 'Rate limit exceeded.' };
    }
    injectRateLimits[id].push(now);

    const validId = validateGameId(id);
    const installPath = gamePathsMap[validId];
    if (!installPath) return { success: false, message: 'Game path not found' };
    
    const logPath = safeGamePath(installPath, 'vrinject.log');
    fs.writeFileSync(logPath, '');
    let lastSize = 0;

    fs.watchFile(logPath, { interval: 500 }, (curr, prev) => {
       if (curr.size > lastSize) {
           try {
             const fd = fs.openSync(logPath, 'r');
             const buf = Buffer.alloc(curr.size - lastSize);
             fs.readSync(fd, buf, 0, buf.length, lastSize);
             fs.closeSync(fd);
             const lines = buf.toString('utf-8').split('\n');
             for (const l of lines) {
                 if (l.trim()) event.sender.send('log:line', l.trim());
             }
             lastSize = curr.size;
           } catch(e) {}
       }
     });

    const dllSource = isDev ? path.join(__dirname, '../../../build/bin/vrinject.dll') : path.join(process.resourcesPath, 'vrinject.dll');
    const dllTarget = path.join(installPath, 'vrinject.dll');
    if (fs.existsSync(dllSource)) {
      fs.copyFileSync(dllSource, dllTarget);
    }
    
    const onnxSource = isDev ? path.join(__dirname, '../../../build/bin/onnxruntime.dll') : path.join(process.resourcesPath, 'onnxruntime.dll');
    if (fs.existsSync(onnxSource)) {
      fs.copyFileSync(onnxSource, path.join(installPath, 'onnxruntime.dll'));
    }
    
    const openxrSource = isDev ? path.join(__dirname, '../../../build/bin/openxr_loader.dll') : path.join(process.resourcesPath, 'openxr_loader.dll');
    if (fs.existsSync(openxrSource)) {
      fs.copyFileSync(openxrSource, path.join(installPath, 'openxr_loader.dll'));
    }
    
    const cliSource = isDev ? path.join(__dirname, '../../../build/bin/vr-inject-cli.exe') : path.join(process.resourcesPath, 'vr-inject-cli.exe');
    const cliTarget = path.join(installPath, 'vr-inject-cli.exe');
    if (fs.existsSync(cliSource)) {
      fs.copyFileSync(cliSource, cliTarget);
    }
    
    const shadersSource = isDev ? path.join(__dirname, '../../../build/bin/shaders') : path.join(process.resourcesPath, 'shaders');
    const modelsSource = isDev ? path.join(__dirname, '../../../build/bin/models') : path.join(process.resourcesPath, 'models');
    
    try {
      if (fs.existsSync(shadersSource)) {
        fs.cpSync(shadersSource, path.join(installPath, 'shaders'), { recursive: true, force: true });
      }
      if (fs.existsSync(modelsSource)) {
        fs.cpSync(modelsSource, path.join(installPath, 'models'), { recursive: true, force: true });
      }
    } catch (e) {
      console.error('Failed to copy shader/model assets to target directory:', e);
    }
    
    if (id.startsWith('custom_') && gameExeMap[id]) {
       child_process.spawn(gameExeMap[id], [], { detached: true, cwd: installPath });
    } else if (/^\d+$/.test(id)) {
       shell.openExternal('steam://rungameid/' + id);
    } else {
       shell.openExternal(`com.epicgames.launcher://apps/${id}?action=launch&silent=true`);
    }
    
    let targetExeName = '';
    if (gameExeMap[id]) {
       targetExeName = path.basename(gameExeMap[id]).toLowerCase();
    } else {
       let largestSize = 0;
       const scan = (dir: string) => {
         try {
           const files = fs.readdirSync(dir);
           for (const f of files) {
             const fullPath = path.join(dir, f);
             try {
               const stats = fs.statSync(fullPath);
               if (stats.isDirectory()) {
                 scan(fullPath);
               } else if (f.toLowerCase().endsWith('.exe')) {
                 const lower = f.toLowerCase();
                 if (lower.includes('launcher') || lower.includes('crash') || lower.includes('reporter') || lower.includes('anticheat') || lower.includes('eosbootstrapper') || lower.includes('start_protected_game')) {
                    continue;
                 }
                 if (stats.size > largestSize) {
                    largestSize = stats.size;
                    targetExeName = f.toLowerCase();
                 }
               }
             } catch(e) {}
           }
         } catch(e) {}
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

    let targetPid = 0;
    let attempts = 0;
    cancelInjectionFlag = false;
    
    while (attempts < 60) {
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
        let maxMem = 0;
        
        for (const line of lines) {
           if (!line.trim()) continue;
           const parts = line.split('","');
           if (parts.length < 5) continue;
           
           const exeName = parts[0].replace(/"/g, '').toLowerCase();
           const pid = parseInt(parts[1].replace(/"/g, ''), 10);
           const memStr = parts[4].replace(/"/g, '').replace(/,/g, '').replace(' K', '');
           const mem = parseInt(memStr, 10);
           
           const baseExeName = exeName.endsWith('.exe') ? exeName.slice(0, -4) : exeName;
           if (exeName === targetExeName || targetExeName.startsWith(baseExeName)) {
              if (mem > maxMem) {
                 maxMem = mem;
                 foundPid = pid;
              }
           }
        }
        
        if (foundPid > 0) {
           await new Promise(r => setTimeout(r, 2000));
           const { stdout: out2 } = await execAsync(`tasklist /fi "PID eq ${foundPid}" /fo csv /nh`, { encoding: 'utf-8', timeout: 2000, killSignal: 'SIGKILL' });
           if (out2.includes(String(foundPid))) {
              targetPid = foundPid;
              activeTargetPid = targetPid;
              break;
           }
        }
      } catch (e) {}
    }
    
    if (!targetPid) {
       fs.unwatchFile(logPath);
       return { success: false, message: 'Game executable not found or closed immediately' };
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

       child_process.execFile(cliTarget, ['--pid', String(targetPid), '--dll', dllTarget], { timeout: 10000, killSignal: 'SIGKILL', cwd: installPath }, (err, stdout, stderr) => {
           if (isResolved) return;
           isResolved = true;
           clearTimeout(fallbackTimeout);

           if (stdout.trim()) event.sender.send('log:line', '[Injector CLI] ' + stdout.trim());
           if (stderr.trim()) event.sender.send('log:line', '[Injector CLI Error] ' + stderr.trim());
           
           try {
             fs.unlinkSync(cliTarget);
           } catch(e) {}
           
           if (err) {
               fs.unwatchFile(logPath);
               resolve({ success: false, message: `CLI failed: ${err.message}` });
           } else {
               resolve({ success: true, message: 'Deployed successfully', pid: targetPid });
           }
       });
    });

  } catch(e: any) {
    return { success: false, message: e.message };
  }
});

ipcMain.handle('inject:monitor', async (_, pid: number) => {
    while (true) {
        try {
            const { stdout } = await execAsync(`tasklist /fi "PID eq ${pid}" /fo csv /nh`, { encoding: 'utf-8', timeout: 2000, killSignal: 'SIGKILL' });
            if (!stdout.includes(String(pid))) {
                break;
            }
        } catch (e) {
            break;
        }
        await new Promise(resolve => setTimeout(resolve, 2000));
    }
});
