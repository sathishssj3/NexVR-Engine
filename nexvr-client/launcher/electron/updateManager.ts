import { app, ipcMain, shell } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
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

const MANIFEST_URLS = [
  'https://raw.githubusercontent.com/sathishssj3/NexVR-Engine/main/updates/manifest.json',
  'https://raw.githubusercontent.com/sathishssj3/NexVR-Engine-Releases/main/updates/manifest.json',
];

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

async function fetchWithTimeout(url: string, timeoutMs = 15000): Promise<Response> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(url, {
      headers: { 'User-Agent': 'NexVR-Launcher' },
      signal: controller.signal,
    });
    return res;
  } finally {
    clearTimeout(timer);
  }
}

async function fetchRemoteManifest(): Promise<{ manifest: UpdateManifest; baseUrl: string } | null> {
  for (const url of MANIFEST_URLS) {
    try {
      console.info(`[UpdateManager] Checking update channel: ${url}`);
      const res = await fetchWithTimeout(url, 15000);
      if (res.status === 404) {
        continue;
      }
      if (!res.ok) {
        throw new Error(`HTTP ${res.status}`);
      }
      const manifest = await res.json() as UpdateManifest;
      const baseUrl = url.substring(0, url.lastIndexOf('/') + 1);
      return { manifest, baseUrl };
    } catch (err: any) {
      console.warn(`[UpdateManager] Channel fetch failed for ${url}:`, err?.message || err);
    }
  }
  return null;
}

async function downloadFileWithFallback(fileName: string, baseUrls: string[], destPath: string): Promise<void> {
  let lastErr: Error | null = null;
  for (const base of baseUrls) {
    const fileUrl = `${base}${fileName}`;
    try {
      const res = await fetchWithTimeout(fileUrl, 30000);
      if (!res.ok) {
        throw new Error(`HTTP ${res.status} from ${fileUrl}`);
      }
      const buffer = Buffer.from(await res.arrayBuffer());
      const tempPath = `${destPath}.tmp`;
      fs.writeFileSync(tempPath, buffer);
      fs.renameSync(tempPath, destPath);
      return;
    } catch (err: any) {
      lastErr = err;
      console.warn(`[UpdateManager] Failed downloading ${fileName} from ${base}:`, err?.message || err);
    }
  }
  throw lastErr || new Error(`Failed to download ${fileName}`);
}

export async function checkForEngineHotfix(): Promise<UpdateStatus> {
  try {
    const remoteData = await fetchRemoteManifest();
    if (!remoteData) {
      console.info('[UpdateManager] Unable to reach update servers, keeping active local build.');
      const local = getLocalManifest();
      return {
        checking: false,
        hasUpdate: false,
        updated: !!local,
        version: local?.engineVersion || '0.1.0',
        changelog: local?.changelog,
        features: local?.features,
        fixes: local?.fixes,
      };
    }

    const { manifest: remote, baseUrl } = remoteData;
    const local = getLocalManifest();

    if (!local || remote.timestamp > local.timestamp) {
      console.info(`[UpdateManager] New engine hotfix available: v${remote.engineVersion} (${remote.changelog})`);
      const updatesDir = getUpdatesDir();

      const candidateBases = [
        baseUrl,
        'https://raw.githubusercontent.com/sathishssj3/NexVR-Engine/main/updates/',
        'https://raw.githubusercontent.com/sathishssj3/NexVR-Engine-Releases/main/updates/',
      ];

      // Download all files in manifest (e.g. vrinject.dll, vr-inject-cli.exe)
      for (const file of remote.files) {
        const dest = path.join(updatesDir, file);
        const parentDir = path.dirname(dest);
        if (!fs.existsSync(parentDir)) {
          fs.mkdirSync(parentDir, { recursive: true });
        }
        await downloadFileWithFallback(file, candidateBases, dest);
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
    console.warn('[UpdateManager] Hotfix check encountered an error:', err.message);
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
