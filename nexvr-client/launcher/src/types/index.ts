export type GraphicsAPI = 'DX11' | 'DX12' | 'Vulkan' | 'Unknown';
export type CompatStatus = 'verified' | 'beta' | 'new' | 'unknown';

export interface GameEntry {
  id:             string;
  name:           string;
  installPath:    string;
  executablePath: string;
  sizeGB:         number;
  api:            GraphicsAPI;
  compat:         CompatStatus;
  hasInjector:    boolean;
  hasAntiCheat?:  boolean;
  antiCheatName?: string;
  iconBase64?:    string;
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

export interface VRConfig {
  motionAimSensitivity:    number;   // 0.1–10.0, default 1.0
  useRecommendedResolution: boolean; // default true
  srgbCorrection:          boolean;  // default true
  depthSubmission:         boolean;  // default false
  rawInputMode:            boolean;  // default true
  autoInjectOnLaunch:      boolean;  // default true
}

export interface VRStatus {
  connected:   boolean;
  runtime:     string;
  headset:     string;
  refreshRate: number;
}

export interface InjectResult {
  success:   boolean;
  message:   string;
  pid?:      number;
  cancelled?: boolean;
}

export interface ScanResult {
  active: GameEntry[];
  waiting: GameEntry[];
}
