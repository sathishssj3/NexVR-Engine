/**
 * /api/report — Automated Telemetry & Bug Reporting Endpoint.
 *
 * Receives bug reports and diagnostic logs from the NexVR Launcher.
 * Stores reports in Cloudflare KV (bound as WAITLIST) and forwards
 * notifications to Discord if a webhook is configured.
 */

const json = (data, status = 200) =>
  new Response(JSON.stringify(data), {
    status,
    headers: {
      'content-type': 'application/json',
      'cache-control': 'no-store',
      'access-control-allow-origin': '*',
      'access-control-allow-methods': 'GET, POST, OPTIONS',
      'access-control-allow-headers': 'Content-Type, Authorization',
    },
  });

export async function onRequestOptions() {
  return new Response(null, {
    status: 204,
    headers: {
      'access-control-allow-origin': '*',
      'access-control-allow-methods': 'GET, POST, OPTIONS',
      'access-control-allow-headers': 'Content-Type, Authorization',
      'access-control-max-age': '86400',
    },
  });
}

export async function onRequestPost({ request, env }) {
  let body;
  try {
    body = await request.json();
  } catch {
    return json({ error: 'Malformed JSON payload.' }, 400);
  }

  const timestamp = Date.now();
  const randomSuffix = Math.random().toString(36).slice(2, 8);
  const reportId = `rep_${timestamp}_${randomSuffix}`;

  const report = {
    id: reportId,
    timestamp,
    date: new Date(timestamp).toISOString(),
    gameId: String(body.gameId || 'manual_session').slice(0, 100),
    gameName: String(body.gameName || body.gameId || 'Manual Report').slice(0, 150),
    engineVersion: String(body.engineVersion || '0.1.90').slice(0, 30),
    status: String(body.status || 'manual_report').slice(0, 30),
    vrHeadset: String(body.vrHeadset || 'Unknown HMD').slice(0, 100),
    vrRuntime: String(body.vrRuntime || 'OpenXR').slice(0, 100),
    message: String(body.message || '').slice(0, 2000),
    userNote: String(body.userNote || '').slice(0, 2000),
    systemSpecs: String(body.systemSpecs || '').slice(0, 2000),
    logSnippet: String(body.logContent || body.logSnippet || '').slice(0, 20000),
    clientIp: request.headers.get('cf-connecting-ip') || 'Unknown',
    country: request.headers.get('cf-ipcountry') || 'Unknown',
  };

  // 1. Store in Cloudflare KV if bound
  if (env.WAITLIST) {
    try {
      // Store full report with 30-day TTL (2,592,000 seconds)
      await env.WAITLIST.put(`REPORT:${reportId}`, JSON.stringify(report), {
        expirationTtl: 2592000,
      });

      // Maintain a ring buffer index of the 40 most recent reports
      let index = [];
      try {
        const rawIndex = await env.WAITLIST.get('REPORTS_INDEX');
        if (rawIndex) index = JSON.parse(rawIndex);
      } catch {}

      index.unshift({
        id: reportId,
        timestamp,
        date: report.date,
        gameName: report.gameName,
        status: report.status,
        vrHeadset: report.vrHeadset,
        engineVersion: report.engineVersion,
        userNote: report.userNote,
        hasLogs: !!report.logSnippet,
      });

      if (index.length > 40) index = index.slice(0, 40);
      await env.WAITLIST.put('REPORTS_INDEX', JSON.stringify(index), {
        expirationTtl: 2592000,
      });
    } catch (err) {
      console.warn('Failed to store report in KV:', err);
    }
  }

  // 2. Forward to Discord Webhook if configured
  const webhookUrl = env.DISCORD_WEBHOOK_URL || body.discordWebhookUrl;
  if (webhookUrl && webhookUrl.startsWith('https://discord.com/api/webhooks/')) {
    try {
      const isError = report.status === 'error';
      const statusEmoji = isError ? '🚨' : '📋';
      const embedColor = isError ? 0xef4444 : 0xaa3bff;

      const fields = [
        { name: 'Game', value: `\`${report.gameName}\``, inline: true },
        { name: 'Engine Version', value: `\`v${report.engineVersion}\``, inline: true },
        { name: 'VR Headset / Runtime', value: `${report.vrHeadset} (${report.vrRuntime})`, inline: false },
      ];

      if (report.userNote) {
        fields.push({ name: 'Tester Note', value: report.userNote, inline: false });
      }

      if (report.message && report.message !== report.userNote) {
        fields.push({ name: 'Details', value: `\`\`\`\n${report.message.slice(0, 500)}\n\`\`\``, inline: false });
      }

      if (report.logSnippet) {
        const lastLines = report.logSnippet.trim().split('\n').slice(-15).join('\n');
        fields.push({ name: 'Recent Log Snippet', value: `\`\`\`\n${lastLines.slice(0, 950)}\n\`\`\``, inline: false });
      }

      const discordPayload = {
        embeds: [
          {
            title: `${statusEmoji} NexVR Beta Report [${report.id}]`,
            color: embedColor,
            fields,
            footer: { text: `Reported from ${report.country} · NexVR Telemetry` },
            timestamp: new Date(timestamp).toISOString(),
          },
        ],
      };

      await fetch(webhookUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(discordPayload),
      });
    } catch (err) {
      console.warn('Discord webhook forwarding error:', err);
    }
  }

  return json({
    success: true,
    reportId,
    message: 'Report received and recorded in engineering telemetry.',
  });
}

export async function onRequestGet({ request, env }) {
  const url = new URL(request.url);
  const reportId = url.searchParams.get('id');

  if (!env.WAITLIST) {
    return json({ error: 'Storage namespace is not configured.' }, 503);
  }

  // Fetch individual report
  if (reportId) {
    const raw = await env.WAITLIST.get(`REPORT:${reportId}`);
    if (!raw) return json({ error: 'Report not found' }, 404);
    return json(JSON.parse(raw));
  }

  // Fetch index of recent reports
  let index = [];
  try {
    const rawIndex = await env.WAITLIST.get('REPORTS_INDEX');
    if (rawIndex) index = JSON.parse(rawIndex);
  } catch {}

  return json({
    count: index.length,
    reports: index,
  });
}
