import { app, ipcMain, shell } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as crypto from 'crypto';
import { assertTrustedIpcSender, resolveWithinRoot } from './utils';

export interface UpdateManifest {
  engineVersion: string;
  timestamp: number;
  changelog: string;
  features?: string[];
  fixes?: string[];
  files: string[];
  hashes?: Record<string, string>;
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
  'https://nexvr-engine.pages.dev/updates/manifest.json',
  'https://cdn.jsdelivr.net/gh/sathishssj3/NexVR-Engine@main/updates/manifest.json',
  'https://fastly.jsdelivr.net/gh/sathishssj3/NexVR-Engine@main/updates/manifest.json',
  'https://gcore.jsdelivr.net/gh/sathishssj3/NexVR-Engine@main/updates/manifest.json',
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

export function getAppVersion(): string {
  if (app?.getVersion) {
    try {
      const v = app.getVersion();
      if (v) return v;
    } catch {}
  }
  try {
    const pkgPath = path.join(__dirname, '..', 'package.json');
    if (fs.existsSync(pkgPath)) {
      const pkg = JSON.parse(fs.readFileSync(pkgPath, 'utf-8'));
      if (pkg.version) return pkg.version;
    }
  } catch {}
  try {
    const rootPkgPath = path.join(process.cwd(), 'package.json');
    if (fs.existsSync(rootPkgPath)) {
      const pkg = JSON.parse(fs.readFileSync(rootPkgPath, 'utf-8'));
      if (pkg.version) return pkg.version;
    }
  } catch {}
  return '0.1.29';
}

export function getLocalManifest(): UpdateManifest | null {
  try {
    const file = path.join(getUpdatesDir(), 'installed_manifest.json');
    if (fs.existsSync(file)) {
      return JSON.parse(fs.readFileSync(file, 'utf-8'));
    }
    const legacy = path.join(getUpdatesDir(), 'manifest.json');
    if (fs.existsSync(legacy)) {
      return JSON.parse(fs.readFileSync(legacy, 'utf-8'));
    }
  } catch {}
  return null;
}

export function purgeStaleOtaCacheIfAppNewer(): void {
  try {
    const local = getLocalManifest();
    const appVer = getAppVersion();
    if (!appVer) return;

    // If local cached OTA belongs to an older version than the installed app,
    // clear outdated cached binaries so the bundled assets are guaranteed to run.
    if (local && compareSemver(appVer, local.engineVersion) > 0) {
      console.info(`[UpdateManager] Current app (v${appVer}) is newer than cached OTA (v${local.engineVersion}). Purging stale OTA cache.`);
      const updatesDir = getUpdatesDir();
      const filesToPurge = ['vrinject.dll', 'vr-inject-cli.exe', 'vrinject.json', 'installed_manifest.json', 'manifest.json'];
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

async function fetchWithTimeout(url: string, timeoutMs = 8000): Promise<Response> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const separator = url.includes('?') ? '&' : '?';
    const cacheBustedUrl = `${url}${separator}_t=${Date.now()}`;
    const res = await fetch(cacheBustedUrl, {
      cache: 'no-store',
      headers: {
        'User-Agent': 'NexVR-Launcher',
        'Cache-Control': 'no-cache, no-store, must-revalidate',
        'Pragma': 'no-cache',
      },
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

async function downloadFileWithFallback(
  fileName: string,
  baseUrls: string[],
  destPath: string,
  expectedHash?: string
): Promise<void> {
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

      // Cryptographic integrity verification against SHA-256 manifest hash
      if (expectedHash) {
        const actualHash = crypto.createHash('sha256').update(buffer).digest('hex');
        if (actualHash.toLowerCase() !== expectedHash.toLowerCase()) {
          // Normalize text line endings (\r\n -> \n and \n -> \r\n) to prevent false-positive CRLF/LF rejections on text assets
          const asString = buffer.toString('utf-8');
          const hashLf = crypto.createHash('sha256').update(Buffer.from(asString.replace(/\r\n/g, '\n'), 'utf-8')).digest('hex');
          const hashCrlf = crypto.createHash('sha256').update(Buffer.from(asString.replace(/\r\n/g, '\n').replace(/\n/g, '\r\n'), 'utf-8')).digest('hex');
          if (hashLf.toLowerCase() !== expectedHash.toLowerCase() && hashCrlf.toLowerCase() !== expectedHash.toLowerCase()) {
            throw new Error(
              `Integrity check failed for ${fileName}: expected SHA-256 ${expectedHash}, got ${actualHash}`
            );
          }
        }
      }

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
      console.warn('[UpdateManager] Unable to reach update servers, keeping active local build.');
      const local = getLocalManifest();
      const fallbackVer = local?.engineVersion || getAppVersion();
      return {
        checking: false,
        hasUpdate: false,
        updated: !!local,
        version: fallbackVer,
        changelog: local?.changelog,
        features: local?.features,
        fixes: local?.fixes,
        error: 'Unable to connect to update servers. Please check your network connection.',
      };
    }

    const { manifest: remote, baseUrl } = remoteData;
    const local = getLocalManifest();
    const appVer = getAppVersion();
    const activeVersion = local?.engineVersion || appVer;
    const versionComp = compareSemver(remote.engineVersion, activeVersion);

    // Prevent older remote hotfix from downgrading a newer packaged installation
    if (versionComp < 0) {
      console.info(`[UpdateManager] Active version v${activeVersion} is newer than remote v${remote.engineVersion}. Skipping hotfix.`);
      return {
        checking: false,
        hasUpdate: false,
        updated: !!local,
        version: activeVersion,
        changelog: local?.changelog || 'Bundled release is up-to-date',
        features: local?.features,
        fixes: local?.fixes,
      };
    }

    // If version is identical: only proceed if local manifest exists and remote has a newer post-release hotfix timestamp
    // On fresh install (!local), the bundled app already possesses all binaries for this release
    if (versionComp === 0) {
      const activeTimestamp = local?.timestamp || 0;
      if (!local || remote.timestamp <= activeTimestamp) {
        console.info(`[UpdateManager] Active version v${activeVersion} is already up-to-date.`);
        return {
          checking: false,
          hasUpdate: false,
          updated: !!local,
          version: activeVersion,
          changelog: local?.changelog || remote.changelog || 'Bundled release is up-to-date',
          features: local?.features || remote.features,
          fixes: local?.fixes || remote.fixes,
        };
      }
    }

    if (!local || compareSemver(remote.engineVersion, local.engineVersion) > 0 || remote.timestamp > local.timestamp) {
      console.info(`[UpdateManager] New engine hotfix available: v${remote.engineVersion} (${remote.changelog})`);
      const updatesDir = getUpdatesDir();

      const candidateBases = [
        baseUrl,
        'https://cdn.jsdelivr.net/gh/sathishssj3/NexVR-Engine@main/updates/',
        'https://fastly.jsdelivr.net/gh/sathishssj3/NexVR-Engine@main/updates/',
        'https://gcore.jsdelivr.net/gh/sathishssj3/NexVR-Engine@main/updates/',
        'https://raw.githubusercontent.com/sathishssj3/NexVR-Engine/main/updates/',
        'https://raw.githubusercontent.com/sathishssj3/NexVR-Engine-Releases/main/updates/',
      ];

      // Download all files in manifest with path traversal protection and integrity verification
      for (const file of remote.files) {
        const dest = resolveWithinRoot(updatesDir, file);
        const parentDir = path.dirname(dest);
        if (!fs.existsSync(parentDir)) {
          fs.mkdirSync(parentDir, { recursive: true });
        }
        const expectedHash = remote.hashes?.[file];
        await downloadFileWithFallback(file, candidateBases, dest, expectedHash);
        console.info(`[UpdateManager] Downloaded & verified hotfix file: ${file}`);
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
      version: local?.engineVersion || getAppVersion(),
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
      version: local?.engineVersion || getAppVersion(),
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
