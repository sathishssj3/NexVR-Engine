import * as fs from 'fs';
import * as path from 'path';
import { getAppVersion } from './updateManager';

export const CLOUDFLARE_TELEMETRY_API =
  process.env.NEXVR_TELEMETRY_API || 'https://nexvr-engine.pages.dev/api/report';

export const DEFAULT_DISCORD_WEBHOOK_URL =
  process.env.NEXVR_TELEMETRY_ENDPOINT ||
  process.env.NEXVR_DISCORD_WEBHOOK ||
  '';

let lastManualReportTimestamp = 0;
const MANUAL_REPORT_COOLDOWN_MS = 3000;

export interface TelemetryPayload {
  gameId: string;
  gameName?: string;
  status: 'started' | 'running' | 'completed' | 'error' | 'manual_report';
  message?: string;
  logContent?: string;
  logFilePath?: string;
  vrRuntime?: string;
  vrHeadset?: string;
  durationSec?: number;
  config?: {
    brightness?: number;
    contrast?: number;
    saturation?: number;
    gamma?: number;
    engineType?: string;
    api?: string;
  };
}

export function sanitizeTelemetry(text: string): string {
  if (!text) return '';
  let s = text;
  // Redact Windows user home paths: C:\Users\<username>\ -> C:\Users\[USER]\
  s = s.replace(/([A-Za-z]:\\Users\\)[^\s\\/"']+/gi, '$1[USER]');
  // Redact UNC user paths
  s = s.replace(/(\\\\[^\s\\/"']+\\Users\\)[^\s\\/"']+/gi, '$1[USER]');
  // Redact IPv4 addresses
  s = s.replace(/\b(?:192\.168\.\d{1,3}\.\d{1,3}|10\.\d{1,3}\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})\b/g, '[REDACTED_IP]');
  return s;
}

export async function sendDiscordTelemetry(
  payload: TelemetryPayload
): Promise<{ success: boolean; message?: string }> {
  // Rate-limiting check on user-triggered manual reports
  if (payload.status === 'manual_report') {
    const now = Date.now();
    if (now - lastManualReportTimestamp < MANUAL_REPORT_COOLDOWN_MS) {
      return { success: false, message: 'Rate limited: please wait a few seconds before submitting another report.' };
    }
    lastManualReportTimestamp = now;
  }

  // Prepare log content if available
  let logText = payload.logContent || '';
  if (!logText && payload.logFilePath && fs.existsSync(payload.logFilePath)) {
    try {
      logText = fs.readFileSync(payload.logFilePath, 'utf-8');
    } catch {}
  }
  const sanitized = sanitizeTelemetry(logText);
  const currentVersion = getAppVersion();

  let cloudflareDelivered = false;
  let discordDelivered = false;

  // 1. Primary: Send to Cloudflare Edge Telemetry API
  try {
    const cfPayload = {
      gameId: payload.gameId,
      gameName: payload.gameName || payload.gameId,
      engineVersion: currentVersion,
      status: payload.status,
      vrHeadset: payload.vrHeadset || 'Unknown HMD',
      vrRuntime: payload.vrRuntime || 'OpenXR',
      message: payload.message || '',
      userNote: payload.status === 'manual_report' ? payload.message : '',
      logContent: sanitized.length > 50000 ? sanitized.slice(-50000) : sanitized,
      durationSec: payload.durationSec,
      config: payload.config,
    };

    const cfRes = await fetch(CLOUDFLARE_TELEMETRY_API, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(cfPayload),
    });
    if (cfRes.ok) {
      cloudflareDelivered = true;
    }
  } catch (err: any) {
    console.warn('[TelemetryManager] Cloudflare delivery failed:', err?.message || err);
  }

  // 2. Secondary: Direct Discord Webhook (if configured)
  const webhookUrl = DEFAULT_DISCORD_WEBHOOK_URL;
  if (webhookUrl && webhookUrl.startsWith('https://discord.com/api/webhooks/')) {
    try {
      const isError = payload.status === 'error';
      const isStarted = payload.status === 'started';
      const isCompleted = payload.status === 'completed';
      const isManual = payload.status === 'manual_report';

      let statusEmoji = '🟢';
      let statusText = 'Running';
      let embedColor = 0x00f0ff; // Cyan

      if (isStarted) {
        statusEmoji = '🚀';
        statusText = 'Session Launched';
        embedColor = 0x3b82f6; // Blue
      } else if (isCompleted) {
        statusEmoji = '🏁';
        statusText = 'Session Completed';
        embedColor = 0x10b981; // Green
      } else if (isError) {
        statusEmoji = '🚨';
        statusText = 'Error / Crash Detected';
        embedColor = 0xef4444; // Red
      } else if (isManual) {
        statusEmoji = '📋';
        statusText = 'Tester Manual Report';
        embedColor = 0xa855f7; // Purple
      }

      const title = `${statusEmoji} NexVR Engine [v${currentVersion}] — ${payload.gameName || payload.gameId}`;

      const fields: Array<{ name: string; value: string; inline?: boolean }> = [
        { name: 'Game ID', value: `\`${payload.gameId}\``, inline: true },
        { name: 'Status', value: `**${statusText}**`, inline: true },
      ];

      if (payload.vrRuntime || payload.vrHeadset) {
        fields.push({
          name: 'VR Device',
          value: `${payload.vrHeadset || 'Unknown HMD'} (${payload.vrRuntime || 'OpenXR'})`,
          inline: true,
        });
      }

      if (payload.durationSec !== undefined && payload.durationSec > 0) {
        const mins = Math.floor(payload.durationSec / 60);
        const secs = Math.floor(payload.durationSec % 60);
        const durationStr = mins > 0 ? `${mins}m ${secs}s` : `${secs}s`;
        fields.push({ name: 'Session Duration', value: durationStr, inline: true });
      }

      if (payload.message) {
        fields.push({
          name: 'Message',
          value: `\`\`\`\n${payload.message.slice(0, 1000)}\n\`\`\``,
        });
      }

      const embedObj = {
        title,
        color: embedColor,
        fields,
        footer: { text: 'NexVR Engine Automated Telemetry Pipeline' },
        timestamp: new Date().toISOString(),
      };

      if (!sanitized) {
        const res = await fetch(webhookUrl, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ embeds: [embedObj] }),
        });
        if (res.ok) discordDelivered = true;
      } else {
        const cappedLog = sanitized.length > 4 * 1024 * 1024 ? sanitized.slice(-4 * 1024 * 1024) : sanitized;
        const formData = new FormData();
        formData.append('payload_json', JSON.stringify({ embeds: [embedObj] }));
        const logFilename = `nexvr_${payload.gameId}_${Date.now()}.log`;
        formData.append('files[0]', new Blob([cappedLog], { type: 'text/plain' }), logFilename);

        const res = await fetch(webhookUrl, {
          method: 'POST',
          body: formData,
        });
        if (res.ok) discordDelivered = true;
      }
    } catch (err: any) {
      console.warn('[TelemetryManager] Direct Discord webhook delivery error:', err?.message || err);
    }
  }

  if (cloudflareDelivered || discordDelivered) {
    return {
      success: true,
      message: 'Diagnostic report recorded in engineering telemetry.',
    };
  }

  return {
    success: false,
    message: 'Unable to connect to telemetry service. Please check your network connection.',
  };
}
