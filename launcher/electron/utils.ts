import { app, dialog } from 'electron';
import * as path from 'path';
import * as fs from 'fs';
import * as child_process from 'child_process';
import { GameEntry, VRConfig } from '../src/types';

export const gamePathsMap: Record<string, string> = {};
export const gameExeMap: Record<string, string> = {};

export function validateGameId(id: unknown): string {
  if (typeof id !== 'string') throw new Error('Invalid gameId type');
  if (!/^[a-zA-Z0-9_]+$/.test(id)) throw new Error('gameId must be alphanumeric/underscore');
  if (id.length > 50) throw new Error('gameId too long');
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
  const resolved = path.resolve(installPath, filename);
  if (!resolved.startsWith(path.resolve(installPath))) throw new Error('Path traversal detected');
  return resolved;
}

export function sanitizeAcfString(val: string): string {
  return val.replace(/[^\x20-\x7E]/g, '').substring(0, 256);
}

export function validateSteamPath(p: string): boolean {
  if (!path.isAbsolute(p)) return false;
  if (p.includes('..')) return false;
  return fs.existsSync(path.join(p, 'steam.exe'));
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
