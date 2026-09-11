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

export function getUpdatesDir(): string {
  const userData = app?.getPath ? app.getPath('userData') : path.join(process.env.APPDATA || process.cwd(), 'NexVR Engine');
  const dir = path.join(userData, 'updates');
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
  return dir;
}

export function parseSemver(v: string): number[] {
  const clean = v.replace(/^v/i, '').trim();
  const parts = clean.split('.').map(p => Number.parseInt(p, 10) || 0);
  while (parts.length < 3) parts.push(0);
  return parts;
}

export function compareSemver(v1: string, v2: string): number {
  const p1 = parseSemver(v1);
  const p2 = parseSemver(v2);
  for (let i = 0; i < 3; i++) {
    if (p1[i] > p2[i]) return 1;
    if (p1[i] < p2[i]) return -1;
  }
  return 0;
}

export function getLocalManifest(): UpdateManifest | null {
  try {
    const file = path.join(getUpdatesDir(), 'installed_manifest.json');
    if (fs.existsSync(file)) {
      return JSON.parse(fs.readFileSync(file, 'utf-8'));
    }
  } catch {}
  return null;
}

export function purgeStaleOtaCacheIfAppNewer(): void {
  try {
    const local = getLocalManifest();
    const appVer = app?.getVersion ? app.getVersion() : '';
    if (!appVer) return;

    // If local cached OTA belongs to an older version than the installed app,
    // clear outdated cached binaries so the bundled assets are guaranteed to run.
    if (local && compareSemver(appVer, local.engineVersion) > 0) {
      console.info(`[UpdateManager] Current app (v${appVer}) is newer than cached OTA (v${local.engineVersion}). Purging stale OTA cache.`);
      const updatesDir = getUpdatesDir();
      const filesToPurge = ['vrinject.dll', 'vr-inject-cli.exe', 'vrinject.json', 'installed_manifest.json'];
      for (const f of filesToPurge) {
        const fp = path.join(updatesDir, f);
        if (fs.existsSync(fp)) {
          try { fs.unlinkSync(fp); } catch {}
        }
      }
      const shadersDir = path.join(updatesDir, 'shaders');
      if (fs.existsSync(shadersDir)) {
        try { fs.rmSync(shadersDir, { recursive: true, force: true }); } catch {}
      }
    }
  } catch (err) {
    console.warn('[UpdateManager] Error during stale OTA cache check:', err);
  }
}

// Purge obsolete OTA cache on module startup
purgeStaleOtaCacheIfAppNewer();

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
  const normalizedFile = fileName.replace(/\\/g, '/');
  for (const base of baseUrls) {
    const fileUrl = `${base.replace(/\/+$/, '')}/${normalizedFile}`;
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
    const appVer = app.getVersion() || '0.1.10';

    // Prevent older remote hotfix from downgrading a newer packaged installation
    if (compareSemver(appVer, remote.engineVersion) > 0) {
      console.info(`[UpdateManager] Remote hotfix v${remote.engineVersion} is older than bundled app v${appVer}. Skipping hotfix.`);
      return {
        checking: false,
        hasUpdate: false,
        updated: false,
        version: appVer,
        changelog: 'Bundled release is up-to-date',
      };
    }

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
if (ipcMain) {
  ipcMain.handle('update:check', async (event) => {
    assertTrustedIpcSender(event);
    return await checkForEngineHotfix();
  });

  ipcMain.handle('update:getStatus', async (event) => {
    assertTrustedIpcSender(event);
    const local = getLocalManifest();
    return {
      version: local?.engineVersion || (app?.getVersion ? app.getVersion() : '0.1.10') || '0.1.10',
      timestamp: local?.timestamp || 0,
      changelog: local?.changelog || '',
      features: local?.features || [],
      fixes: local?.fixes || [],
    };
  });

  ipcMain.handle('update:openFolder', async (event) => {
    assertTrustedIpcSender(event);
    const dir = getUpdatesDir();
    if (shell?.openPath) shell.openPath(dir);
  });
}
