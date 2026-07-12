import { app } from 'electron';
import type { IpcMainEvent, IpcMainInvokeEvent } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import { VRConfig } from '../src/types';

export const gamePathsMap: Record<string, string> = {};
export const gameExeMap: Record<string, string> = {};

export function assertTrustedIpcSender(event: IpcMainInvokeEvent | IpcMainEvent): void {
  const senderUrl = event.senderFrame?.url || event.sender.getURL();

  if (app.isPackaged) {
    if (!senderUrl.startsWith('nexvr://app/')) {
      throw new Error('Rejected IPC from an untrusted frame');
    }
    return;
  }

  const devServerUrl = process.env.VITE_DEV_SERVER_URL;
  if (!devServerUrl) throw new Error('Development server URL is not configured');

  const sender = new URL(senderUrl);
  const allowed = new URL(devServerUrl);
  if (sender.origin !== allowed.origin) {
    throw new Error('Rejected IPC from an untrusted development origin');
  }
}

export function validateGameId(id: unknown): string {
  if (typeof id !== 'string') throw new Error('Invalid gameId type');
  if (!/^[a-zA-Z0-9_.-]+$/.test(id)) throw new Error('gameId contains invalid characters');
  if (id.length > 128) throw new Error('gameId too long');
  return id;
}

export function validateConfig(cfg: unknown): VRConfig {
  if (typeof cfg !== 'object' || cfg === null) throw new Error('Invalid config');
  const c = cfg as Record<string, unknown>;
  const sens = Number(c.motionAimSensitivity);
  if (isNaN(sens) || sens < 0.1 || sens > 10.0) throw new Error('motionAimSensitivity out of range');
  return {
    motionAimSensitivity: sens,
    useRecommendedResolution: Boolean(c.useRecommendedResolution),
    srgbCorrection: Boolean(c.srgbCorrection),
    depthSubmission: Boolean(c.depthSubmission),
    rawInputMode: Boolean(c.rawInputMode),
    autoInjectOnLaunch: Boolean(c.autoInjectOnLaunch),
  };
}

export function safeGamePath(installPath: string, filename: string): string {
  if (/[\/\\:*?"<>|]/.test(filename)) throw new Error(`Invalid filename: ${filename}`);
  return resolveWithinRoot(installPath, filename);
}

export function resolveWithinRoot(rootPath: string, ...segments: string[]): string {
  const root = path.resolve(rootPath);
  const resolved = path.resolve(root, ...segments);
  const relative = path.relative(root, resolved);
  if (relative === '' || (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative))) {
    return resolved;
  }
  throw new Error('Path traversal detected');
}

export function canonicalExistingPath(inputPath: string, expected: 'file' | 'directory'): string {
  if (typeof inputPath !== 'string' || !path.isAbsolute(inputPath)) {
    throw new Error('Path must be absolute');
  }
  const canonical = fs.realpathSync.native(inputPath);
  const stat = fs.statSync(canonical);
  if (expected === 'file' ? !stat.isFile() : !stat.isDirectory()) {
    throw new Error(`Expected an existing ${expected}`);
  }
  return canonical;
}

export function sanitizeAcfString(val: string): string {
  return val.replace(/[^\x20-\x7E]/g, '').substring(0, 256);
}

export function validateSteamPath(p: string): boolean {
  try {
    const canonical = canonicalExistingPath(p, 'directory');
    return fs.existsSync(path.join(canonical, 'steam.exe'));
  } catch {
    return false;
  }
}

export const isIgnoredSoftware = (name: string): boolean => {
  const lower = name.toLowerCase();
  return lower.includes('steamworks common redistributables') ||
         lower.includes('proton') ||
         lower.includes('linux runtime') ||
         lower.includes('dedicated server') ||
         lower.includes('soundtrack') ||
         lower.includes('steamvr') ||
         lower.includes('unreal engine') ||
         lower.includes('fab ue plugin') ||
         lower.includes('quixel bridge') ||
         lower.includes('twinmotion');
};
