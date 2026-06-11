import { ipcMain } from 'electron';
import * as fs from 'fs';
import { VRConfig } from '../src/types';
import { gamePathsMap, validateGameId, validateConfig, safeGamePath } from './utils';

const defaultVRConfig: VRConfig = {
  motionAimSensitivity: 1.0,
  useRecommendedResolution: true,
  srgbCorrection: true,
  depthSubmission: false,
  rawInputMode: true,
  autoInjectOnLaunch: true,
};

ipcMain.handle('config:read', async (_, id: string): Promise<VRConfig> => {
  try {
    const validId = validateGameId(id);
    const installPath = gamePathsMap[validId];
    if (!installPath) return defaultVRConfig;
    const cfgPath = safeGamePath(installPath, 'vrinject.json');
    if (fs.existsSync(cfgPath)) {
      const content = fs.readFileSync(cfgPath, 'utf-8');
      const parsed = JSON.parse(content);
      return { ...defaultVRConfig, ...parsed };
    }
    return defaultVRConfig;
  } catch (e) {
    console.error('config:read error', e);
    return defaultVRConfig;
  }
});

ipcMain.handle('config:write', async (_, id: string, cfg: unknown) => {
  try {
    const cfgString = JSON.stringify(cfg);
    if (cfgString.length > 10240) throw new Error('Payload too large (max 10KB)');
    
    const validId = validateGameId(id);
    const validCfg = validateConfig(cfg);
    const installPath = gamePathsMap[validId];
    if (!installPath) return { success: false, error: 'Game path not found' };
    const cfgPath = safeGamePath(installPath, 'vrinject.json');
    fs.writeFileSync(cfgPath, JSON.stringify(validCfg, null, 2));
    fs.chmodSync(cfgPath, 0o600); // S6.3: Owner rw only
    return { success: true };
  } catch (e: any) {
    return { success: false, error: e.message };
  }
});
