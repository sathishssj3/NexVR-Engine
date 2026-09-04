import { app, ipcMain, shell } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as https from 'https';
import { assertTrustedIpcSender } from './utils';

export interface UpdateManifest {
  engineVersion: string;
  timestamp: number;
  changelog: string;
  features?: string[];
  fixes?: string[];
  files: string[];
}

export interface UpdateStatus {
  checking: boolean;
  hasUpdate: boolean;
  updated: boolean;
  version: string;
  changelog?: string;
  features?: string[];
  fixes?: string[];
  error?: string;
}

const MANIFEST_URL = 'https://raw.githubusercontent.com/sathishssj3/NexVR-Engine-Releases/main/updates/manifest.json';
const RAW_FILES_BASE = 'https://raw.githubusercontent.com/sathishssj3/NexVR-Engine-Releases/main/updates/';

function getUpdatesDir(): string {
  const dir = path.join(app.getPath('userData'), 'updates');
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
  return dir;
}

function getLocalManifest(): UpdateManifest | null {
  try {
    const file = path.join(getUpdatesDir(), 'installed_manifest.json');
    if (fs.existsSync(file)) {
      return JSON.parse(fs.readFileSync(file, 'utf-8'));
    }
  } catch {}
  return null;
}

function fetchJson<T>(url: string): Promise<T> {
  return new Promise((resolve, reject) => {
    const req = https.get(url, { headers: { 'User-Agent': 'NexVR-Launcher' }, timeout: 5000 }, (res) => {
      if (res.statusCode === 404) {
        resolve(null as any);
        res.resume();
        return;
      }
      if (res.statusCode !== 200) {
        reject(new Error(`HTTP ${res.statusCode} from ${url}`));
        res.resume();
        return;
      }
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => {
        try {
          resolve(JSON.parse(data) as T);
        } catch (e) {
          reject(e);
        }
      });
    });
    req.on('error', reject);
    req.on('timeout', () => {
      req.destroy();
      reject(new Error('Request timed out'));
    });
  });
}

function downloadFile(url: string, destPath: string): Promise<void> {
  return new Promise((resolve, reject) => {
    const tempPath = `${destPath}.tmp`;
    const fileStream = fs.createWriteStream(tempPath);
    
    const req = https.get(url, { headers: { 'User-Agent': 'NexVR-Launcher' }, timeout: 15000 }, (res) => {
      if (res.statusCode !== 200) {
        fileStream.close();
        fs.unlink(tempPath, () => {});
        reject(new Error(`HTTP ${res.statusCode} downloading ${url}`));
        res.resume();
        return;
      }
      res.pipe(fileStream);
      fileStream.on('finish', () => {
        fileStream.close(() => {
          try {
            fs.renameSync(tempPath, destPath);
            resolve();
          } catch (err) {
            reject(err);
          }
        });
      });
    });
    req.on('error', (err) => {
      fileStream.close();
      fs.unlink(tempPath, () => {});
      reject(err);
    });
    req.on('timeout', () => {
      req.destroy();
      fileStream.close();
      fs.unlink(tempPath, () => {});
      reject(new Error('Download timed out'));
    });
  });
}

export async function checkForEngineHotfix(): Promise<UpdateStatus> {
  try {
    const remote = await fetchJson<UpdateManifest>(MANIFEST_URL);
    if (!remote) {
      console.info('[UpdateManager] System is running the latest engine build.');
      const local = getLocalManifest();
      return {
        checking: false,
        hasUpdate: false,
        updated: false,
        version: local?.engineVersion || '0.1.0',
      };
    }
    const local = getLocalManifest();

    if (!local || remote.timestamp > local.timestamp) {
      console.info(`[UpdateManager] New engine hotfix available: v${remote.engineVersion} (${remote.changelog})`);
      const updatesDir = getUpdatesDir();

      // Download all files in manifest (e.g. vrinject.dll, shaders)
      for (const file of remote.files) {
        const fileUrl = `${RAW_FILES_BASE}${file}`;
        const dest = path.join(updatesDir, file);
        const parentDir = path.dirname(dest);
        if (!fs.existsSync(parentDir)) {
          fs.mkdirSync(parentDir, { recursive: true });
        }
        await downloadFile(fileUrl, dest);
        console.info(`[UpdateManager] Downloaded hotfix file: ${file}`);
      }

      // Save installed manifest
      fs.writeFileSync(path.join(updatesDir, 'installed_manifest.json'), JSON.stringify(remote, null, 2));
      console.info('[UpdateManager] Hotfix installation complete!');

      return {
        checking: false,
        hasUpdate: true,
        updated: true,
        version: remote.engineVersion,
        changelog: remote.changelog,
        features: remote.features,
        fixes: remote.fixes,
      };
    } else {
      return {
        checking: false,
        hasUpdate: false,
        updated: true,
        version: local.engineVersion,
        changelog: local.changelog,
        features: local.features,
        fixes: local.fixes,
      };
    }
  } catch (err: any) {
    console.warn('[UpdateManager] Hotfix check skipped (offline or network error):', err.message);
    const local = getLocalManifest();
    return {
      checking: false,
      hasUpdate: false,
      updated: !!local,
      version: local?.engineVersion || '0.1.0',
      changelog: local?.changelog,
      features: local?.features,
      fixes: local?.fixes,
      error: err.message,
    };
  }
}

// IPC Handlers
ipcMain.handle('update:check', async (event) => {
  assertTrustedIpcSender(event);
  return await checkForEngineHotfix();
});

ipcMain.handle('update:getStatus', async (event) => {
  assertTrustedIpcSender(event);
  const local = getLocalManifest();
  return {
    version: local?.engineVersion || '0.1.0',
    timestamp: local?.timestamp || 0,
    changelog: local?.changelog || '',
    features: local?.features || [],
    fixes: local?.fixes || [],
  };
});

ipcMain.handle('update:openFolder', async (event) => {
  assertTrustedIpcSender(event);
  const dir = getUpdatesDir();
  shell.openPath(dir);
});
