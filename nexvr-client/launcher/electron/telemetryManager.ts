import * as fs from 'fs';
import * as path from 'path';

export const DEFAULT_DISCORD_WEBHOOK_URL =
  process.env.NEXVR_TELEMETRY_ENDPOINT ||
  process.env.NEXVR_DISCORD_WEBHOOK ||
  'https://discord.com/api/webhooks/1548659339953176636/mQ43f5Q0-RklKr1-P1edOLw7WptaVw1g2sZpg_bWuVawQVPVcNUj5zYC3KLiDuZgFRGB';

let lastManualReportTimestamp = 0;
const MANUAL_REPORT_COOLDOWN_MS = 5000;

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
  const webhookUrl = DEFAULT_DISCORD_WEBHOOK_URL;
  if (!webhookUrl || (!webhookUrl.startsWith('https://discord.com/api/webhooks/') && !webhookUrl.startsWith('https://'))) {
    return { success: false, message: 'Invalid or missing Telemetry Endpoint URL' };
  }

  // Rate-limiting check on user-triggered manual reports
  if (payload.status === 'manual_report') {
    const now = Date.now();
    if (now - lastManualReportTimestamp < MANUAL_REPORT_COOLDOWN_MS) {
      return { success: false, message: 'Rate limited: please wait a few seconds before submitting another report.' };
    }
    lastManualReportTimestamp = now;
  }

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

    const title = `${statusEmoji} NexVR Engine [v0.1.23] — ${payload.gameName || payload.gameId}`;

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

    if (payload.config) {
      const c = payload.config;
      const calibStr = [
        c.brightness !== undefined ? `Brightness: \`${c.brightness}\`` : '',
        c.contrast !== undefined ? `Contrast: \`${c.contrast}\`` : '',
        c.saturation !== undefined ? `Saturation: \`${c.saturation}\`` : '',
        c.engineType ? `Profile: \`${c.engineType}\`` : '',
        c.api ? `API: \`${c.api}\`` : '',
      ]
        .filter(Boolean)
        .join(' | ');

      if (calibStr) {
        fields.push({ name: 'Engine Calibration & Profile', value: calibStr });
      }
    }

    if (payload.message) {
      fields.push({
        name: 'Message',
        value: `\`\`\`\n${payload.message.slice(0, 1000)}\n\`\`\``,
      });
    }

    // Prepare log content if available
    let logText = payload.logContent || '';
    if (!logText && payload.logFilePath && fs.existsSync(payload.logFilePath)) {
      try {
        logText = fs.readFileSync(payload.logFilePath, 'utf-8');
      } catch {}
    }

    const embedObj = {
      title,
      color: embedColor,
      fields,
      footer: { text: 'NexVR Engine Automated Telemetry Pipeline' },
      timestamp: new Date().toISOString(),
    };

    if (!logText) {
      // Send JSON payload only
      const res = await fetch(webhookUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ embeds: [embedObj] }),
      });
      return { success: res.ok, message: `HTTP ${res.status}` };
    }

    // Sanitize and cap log file size to 4MB max for Discord webhooks
    const sanitized = sanitizeTelemetry(logText);
    const cappedLog =
      sanitized.length > 4 * 1024 * 1024
        ? sanitized.slice(-4 * 1024 * 1024)
        : sanitized;

    const formData = new FormData();
    formData.append(
      'payload_json',
      JSON.stringify({
        embeds: [embedObj],
      })
    );

    const logFilename = `nexvr_${payload.gameId}_${Date.now()}.log`;
    formData.append(
      'files[0]',
      new Blob([cappedLog], { type: 'text/plain' }),
      logFilename
    );

    const res = await fetch(webhookUrl, {
      method: 'POST',
      body: formData,
    });

    return {
      success: res.ok,
      message: res.ok ? 'Log delivered to Discord' : `Discord returned HTTP ${res.status}`,
    };
  } catch (err: any) {
    return { success: false, message: err?.message || String(err) };
  }
}
