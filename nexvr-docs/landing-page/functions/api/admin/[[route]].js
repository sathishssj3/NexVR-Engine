/**
 * /api/admin/* — everything the dashboard needs, behind one auth check.
 *
 * A single catch-all rather than a file per endpoint, so there is exactly one
 * place where "is this caller allowed" is decided. Splitting it across four
 * files is four chances to forget the guard on one of them.
 *
 *   POST /api/admin/login          { password }        -> sets session cookie
 *   POST /api/admin/logout                             -> clears it
 *   GET  /api/admin/session                            -> { authed }
 *   GET  /api/admin/stats?days=30                      -> the whole dashboard
 *   GET  /api/admin/waitlist.csv                       -> addresses, as a file
 */

import { json, dayOffset } from '../../_lib/analytics.js';
import {
  verifySession, issueSession, sessionCookie, clearCookie,
  timingSafeEqual, loginThrottled, clearLoginThrottle,
} from '../../_lib/auth.js';
import { buildEmailHtml, buildPlainText, DEFAULT_CONFIG } from '../../_lib/email-template.js';

const RANGES = new Set([7, 30, 90]);
const GITHUB_REPO = 'sathishssj3/NexVR-Engine-Releases';

export async function onRequest(ctx) {
  const { request, env, params } = ctx;
  const route = (Array.isArray(params.route) ? params.route.join('/') : params.route) || '';
  const method = request.method;

  if (!env.ADMIN_PASSWORD) {
    console.error('ADMIN_PASSWORD is not set — the admin API is disabled.');
    return json({ error: 'Admin is not configured on this deployment.' }, 503);
  }

  /* ---------- unauthenticated routes ---------- */

  if (route === 'login' && method === 'POST') {
    if (await loginThrottled(env, request)) {
      return json({ error: 'Too many attempts. Wait 15 minutes.' }, 429);
    }
    let body;
    try { body = await request.json(); } catch { body = {}; }

    if (!timingSafeEqual(String(body.password ?? ''), env.ADMIN_PASSWORD)) {
      // Deliberately identical for "no password" and "wrong password".
      return json({ error: 'Incorrect password.' }, 401);
    }
    await clearLoginThrottle(env, request);
    return json({ ok: true }, 200, {
      'set-cookie': sessionCookie(request, await issueSession(env)),
    });
  }

  if (route === 'logout' && method === 'POST') {
    return json({ ok: true }, 200, { 'set-cookie': clearCookie(request) });
  }

  /* ---------- everything below needs a valid session ---------- */

  const authed = await verifySession(env, request);
  if (route === 'session') return json({ authed });
  if (!authed) return json({ error: 'Not signed in.' }, 401);

  if (route === 'stats' && method === 'GET') return stats(request, env);
  if (route === 'waitlist.csv' && method === 'GET') return waitlistCsv(env);
  if (route === 'waitlist/delete' && method === 'POST') return deleteWaitlistEmail(request, env);
  if (route === 'reset-data' && method === 'POST') return resetData(request, env);
  if (route === 'send-email' && method === 'POST') return sendEmail(request, env);

  return json({ error: 'Not found.' }, 404);
}

/* ============================================================ */

async function stats(request, env) {
  const asked = Number(new URL(request.url).searchParams.get('days') || 30);
  const days = RANGES.has(asked) ? asked : 30;

  const from = dayOffset(days - 1);          // inclusive, covers today
  const prevFrom = dayOffset(days * 2 - 1);  // previous window of equal length

  const [traffic, waitlist, github] = await Promise.all([
    trafficStats(env, from, prevFrom, days),
    waitlistStats(env),
    githubDownloads(env),
  ]);

  return json({ days, from, generatedAt: new Date().toISOString(), ...traffic, waitlist, github });
}

async function trafficStats(env, from, prevFrom, days) {
  if (!env.ANALYTICS) {
    return { configured: false, note: 'No D1 binding named ANALYTICS on this deployment.' };
  }

  const q = (sql, ...binds) => env.ANALYTICS.prepare(sql).bind(...binds);

  const TOTALS = `
    SELECT
      SUM(CASE WHEN type='view'     THEN 1 ELSE 0 END) AS views,
      COUNT(DISTINCT CASE WHEN type='view' THEN visitor END) AS uniques,
      SUM(CASE WHEN type='download' THEN 1 ELSE 0 END) AS downloads,
      SUM(CASE WHEN type='waitlist' THEN 1 ELSE 0 END) AS signups
    FROM events`;

  try {
    const r = await env.ANALYTICS.batch([
      // 0 — daily series
      q(`SELECT day,
           SUM(CASE WHEN type='view'     THEN 1 ELSE 0 END) AS views,
           COUNT(DISTINCT CASE WHEN type='view' THEN visitor END) AS uniques,
           SUM(CASE WHEN type='download' THEN 1 ELSE 0 END) AS downloads,
           SUM(CASE WHEN type='waitlist' THEN 1 ELSE 0 END) AS signups
         FROM events WHERE day >= ?1 GROUP BY day ORDER BY day`, from),
      // 1 — this window
      q(`${TOTALS} WHERE day >= ?1`, from),
      // 2 — the window before it, for the deltas
      q(`${TOTALS} WHERE day >= ?1 AND day < ?2`, prevFrom, from),
      // 3 — traffic sources
      q(`SELECT COALESCE(ref_host,'(direct)') AS src, COUNT(*) AS c,
                COUNT(DISTINCT visitor) AS u
         FROM events WHERE type='view' AND day >= ?1
         GROUP BY src ORDER BY u DESC, c DESC LIMIT 12`, from),
      // 4 — countries
      q(`SELECT COALESCE(country,'??') AS cc, COUNT(DISTINCT visitor) AS u, COUNT(*) AS c
         FROM events WHERE type='view' AND day >= ?1
         GROUP BY cc ORDER BY u DESC LIMIT 12`, from),
      // 5 — devices
      q(`SELECT COALESCE(device,'unknown') AS d, COUNT(DISTINCT visitor) AS u
         FROM events WHERE type='view' AND day >= ?1 GROUP BY d ORDER BY u DESC`, from),
      // 6 — which button they pressed
      q(`SELECT COALESCE(json_extract(meta,'$.to'),'?') AS target,
                COUNT(*) AS c, COUNT(DISTINCT visitor) AS u
         FROM events WHERE type='download' AND day >= ?1
         GROUP BY target ORDER BY c DESC`, from),
      // 7 — how far down the page people actually get
      q(`SELECT json_extract(meta,'$.id') AS sec, COUNT(DISTINCT visitor) AS u
         FROM events WHERE type='section' AND day >= ?1 AND meta IS NOT NULL
         GROUP BY sec ORDER BY u DESC LIMIT 20`, from),
      // 8 — dwell buckets. Buckets rather than a mean: one tab left open for
      //     half an hour would drag an average somewhere untrue.
      q(`SELECT CASE WHEN s < 10 THEN '0-10s'
                     WHEN s < 60 THEN '10-60s'
                     WHEN s < 300 THEN '1-5m'
                     ELSE '5m+' END AS bucket, COUNT(*) AS c
         FROM (SELECT CAST(json_extract(meta,'$.s') AS INTEGER) AS s
               FROM events WHERE type='dwell' AND day >= ?1)
         GROUP BY bucket`, from),
      // 9 — hour of day, UTC
      q(`SELECT CAST(strftime('%H', ts, 'unixepoch') AS INTEGER) AS hr, COUNT(*) AS c
         FROM events WHERE type='view' AND day >= ?1 GROUP BY hr ORDER BY hr`, from),
      // 10 — since the very first event
      q(`SELECT MIN(day) AS since,
                SUM(CASE WHEN type='view'     THEN 1 ELSE 0 END) AS views,
                SUM(CASE WHEN type='download' THEN 1 ELSE 0 END) AS downloads
         FROM events`),
    ]);

    const rows = (i) => r[i]?.results ?? [];
    const one = (i) => rows(i)[0] ?? {};

    return {
      configured: true,
      series: fillGaps(rows(0), days),
      totals: numeric(one(1)),
      previous: numeric(one(2)),
      referrers: rows(3),
      countries: rows(4),
      devices: rows(5),
      downloads: rows(6),
      sections: rows(7),
      dwell: rows(8),
      hours: rows(9),
      allTime: numeric(one(10)),
    };
  } catch (err) {
    console.error('stats query failed:', err?.message || err);
    return { configured: false, note: 'Analytics tables are missing. Apply schema.sql to D1.' };
  }
}

/** SQL only returns days that had traffic; a chart needs the quiet ones too. */
function fillGaps(rows, days) {
  const byDay = new Map(rows.map((r) => [r.day, r]));
  const out = [];
  for (let i = days - 1; i >= 0; i--) {
    const day = dayOffset(i);
    const r = byDay.get(day);
    out.push({
      day,
      views: r?.views ?? 0,
      uniques: r?.uniques ?? 0,
      downloads: r?.downloads ?? 0,
      signups: r?.signups ?? 0,
    });
  }
  return out;
}

const numeric = (o) =>
  Object.fromEntries(Object.entries(o).map(([k, v]) => [k, typeof v === 'number' ? v : (v ?? 0)]));

/* ---------- waitlist lives in KV, not D1 ---------- */

async function listWaitlist(env, limit = 5000) {
  const out = [];
  let cursor;
  do {
    const page = await env.WAITLIST.list({ prefix: 'email:', limit: 1000, cursor });
    for (const k of page.keys) {
      out.push({
        email: k.name.slice(6),
        at: k.metadata?.at ?? null,
        country: k.metadata?.country ?? null,
        // Entries written before metadata was recorded have no date. Rather
        // than fetch each one, they are shown as unknown — the address, which
        // is the part that matters, is in the key either way.
      });
    }
    cursor = page.list_complete ? null : page.cursor;
  } while (cursor && out.length < limit);
  return out;
}

async function waitlistStats(env) {
  if (!env.WAITLIST) return { configured: false, total: 0, recent: [] };
  try {
    const all = await listWaitlist(env);
    all.sort((a, b) => String(b.at ?? '').localeCompare(String(a.at ?? '')));
    return { configured: true, total: all.length, recent: all.slice(0, 500) };
  } catch (err) {
    console.error('waitlist read failed:', err?.message || err);
    return { configured: false, total: 0, recent: [] };
  }
}

async function waitlistCsv(env) {
  if (!env.WAITLIST) return json({ error: 'Waitlist store is not bound.' }, 503);
  const all = await listWaitlist(env);
  all.sort((a, b) => String(a.at ?? '').localeCompare(String(b.at ?? '')));

  // Prefix anything Excel would evaluate as a formula. An address is never
  // meant to execute when someone opens the export.
  const cell = (v) => {
    const s = String(v ?? '');
    const safe = /^[=+\-@\t\r]/.test(s) ? `'${s}` : s;
    return `"${safe.replace(/"/g, '""')}"`;
  };

  const body = ['email,signed_up_at,country']
    .concat(all.map((r) => [cell(r.email), cell(r.at), cell(r.country)].join(',')))
    .join('\r\n');

  return new Response(body, {
    headers: {
      'content-type': 'text/csv; charset=utf-8',
      'content-disposition': `attachment; filename="nexvr-waitlist-${new Date().toISOString().slice(0, 10)}.csv"`,
      'cache-control': 'no-store',
    },
  });
}

/* ---------- remove single waitlist email ---------- */

async function deleteWaitlistEmail(request, env) {
  if (!env.WAITLIST) return json({ error: 'Waitlist store is not bound.' }, 503);
  let body;
  try { body = await request.json(); } catch { body = {}; }
  const email = (body.email || '').trim().toLowerCase();
  if (!email) return json({ error: 'Email address is required.' }, 400);

  await env.WAITLIST.delete(`email:${email}`);
  return json({ ok: true, email, message: `Removed ${email} from waitlist.` });
}

/* ---------- data reset (analytics events and/or waitlist) ---------- */

async function resetData(request, env) {
  let body;
  try { body = await request.json(); } catch { body = {}; }
  const target = body.target || 'analytics'; // 'analytics' | 'waitlist' | 'all'

  let analyticsCleared = false;
  let waitlistCount = 0;

  // 1. Reset Analytics Data (Events table in D1)
  if (target === 'analytics' || target === 'all') {
    if (env.ANALYTICS) {
      try {
        await env.ANALYTICS.prepare('DELETE FROM events').run();
        analyticsCleared = true;
      } catch (err) {
        console.error('Failed to clear events:', err?.message || err);
      }
    }
  }

  // 2. Reset Waitlist Data (email:* keys in KV)
  if (target === 'waitlist' || target === 'all') {
    if (env.WAITLIST) {
      try {
        const all = await listWaitlist(env);
        for (const item of all) {
          await env.WAITLIST.delete(`email:${item.email}`);
        }
        waitlistCount = all.length;
      } catch (err) {
        console.error('Failed to clear waitlist:', err?.message || err);
      }
    }
  }

  return json({
    ok: true,
    target,
    analyticsCleared,
    waitlistCount,
    message: target === 'all'
      ? `Full data reset complete: analytics events cleared and ${waitlistCount} waitlist subscribers removed.`
      : target === 'analytics'
      ? 'Analytics events data successfully reset to 0.'
      : `${waitlistCount} waitlist subscriber(s) removed.`,
  });
}

/* ---------- email broadcast & test send via Resend API ---------- */

async function sendEmail(request, env) {
  let body;
  try {
    body = await request.json();
  } catch {
    body = {};
  }

  const apiKey = (body.apiKey || env.RESEND_API_KEY || '').trim();
  if (!apiKey) {
    return json({ error: 'RESEND_API_KEY is not configured on this server.' }, 400);
  }

  const fromEmail = (body.fromEmail || env.FROM_EMAIL || 'onboarding@resend.dev').trim();
  const fromName = 'Stereo Engine';
  const version = (body.version || 'v0.1.3').trim();
  const subject = (body.subject || `Stereo Engine ${version} — Early Access Build`).trim();
  const isBroadcast = Boolean(body.isBroadcast);
  const recipient = (body.recipient || '').trim();

  const websiteUrl = DEFAULT_CONFIG.websiteUrl;

  // Mode A: Single / Test Send
  if (!isBroadcast) {
    if (!recipient || !recipient.includes('@')) {
      return json({ error: 'A valid recipient email address is required for test send.' }, 400);
    }

    try {
      const res = await fetch('https://api.resend.com/emails', {
        method: 'POST',
        headers: {
          'Authorization': `Bearer ${apiKey}`,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          from: `${fromName} <${fromEmail}>`,
          to: [recipient],
          subject,
          html: buildEmailHtml(recipient, version),
          text: buildPlainText(recipient, version),
          headers: {
            'List-Unsubscribe': `<mailto:unsubscribe@resend.dev?subject=unsubscribe>, <${websiteUrl}#unsubscribe>`,
            'List-Unsubscribe-Post': 'List-Unsubscribe=One-Click',
          },
        }),
      });

      if (!res.ok) {
        const errText = await res.text();
        let parsed = null;
        try { parsed = JSON.parse(errText); } catch {}
        if (parsed?.message && parsed.message.includes('You can only send testing emails to your own email address')) {
          return json({
            error: `Resend Sandbox Limit: 'onboarding@resend.dev' can only send emails to your registered account (${parsed.message.match(/\(([^)]+)\)/)?.[1] || 'sathish6383349@gmail.com'}). To send to '${recipient}', please verify a custom domain at resend.com/domains.`,
            isSandboxRestriction: true,
          }, 403);
        }
        return json({ error: `Resend API error: ${parsed?.message || errText}` }, 400);
      }

      const data = await res.json();
      return json({
        ok: true,
        isBroadcast: false,
        recipient,
        messageId: data.id,
        message: `Test email successfully sent to ${recipient}!`,
      });
    } catch (err) {
      return json({ error: err.message || 'Network error communicating with Resend.' }, 500);
    }
  }

  // Mode B: Live Broadcast to All Waitlist Subscribers
  if (!env.WAITLIST) {
    return json({ error: 'WAITLIST KV store is not configured.' }, 500);
  }

  const subscribers = await listWaitlist(env);
  if (!subscribers.length) {
    return json({ error: 'No subscribers found on waitlist.' }, 400);
  }

  let sent = 0;
  let failed = 0;
  const errors = [];

  for (const sub of subscribers) {
    try {
      const res = await fetch('https://api.resend.com/emails', {
        method: 'POST',
        headers: {
          'Authorization': `Bearer ${apiKey}`,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          from: `${fromName} <${fromEmail}>`,
          to: [sub.email],
          subject,
          html: buildEmailHtml(sub.email, version),
          text: buildPlainText(sub.email, version),
          headers: {
            'List-Unsubscribe': `<mailto:unsubscribe@resend.dev?subject=unsubscribe>, <${websiteUrl}#unsubscribe>`,
            'List-Unsubscribe-Post': 'List-Unsubscribe=One-Click',
          },
        }),
      });

      if (res.ok) {
        sent++;
      } else {
        const errText = await res.text();
        failed++;
        errors.push(`${sub.email}: ${errText}`);
      }
    } catch (err) {
      failed++;
      errors.push(`${sub.email}: ${err.message}`);
    }
  }

  return json({
    ok: true,
    isBroadcast: true,
    total: subscribers.length,
    sent,
    failed,
    errors: errors.slice(0, 5),
    message: `Broadcast complete: ${sent} sent, ${failed} failed out of ${subscribers.length} subscribers.`,
  });
}

/* ---------- real download counts, straight from GitHub ---------- */

/**
 * Click-throughs and completed downloads are different numbers, and the gap
 * between them is worth seeing. Cached for an hour: GitHub's unauthenticated
 * limit is 60 requests an hour and it is shared across the whole Worker.
 */
async function githubDownloads(env) {
  const CACHE_KEY = 'cache:gh-releases';
  if (env.WAITLIST) {
    const hit = await env.WAITLIST.get(CACHE_KEY, 'json');
    if (hit) return { ...hit, cached: true };
  }
  try {
    const res = await fetch(`https://api.github.com/repos/${GITHUB_REPO}/releases?per_page=20`, {
      headers: { accept: 'application/vnd.github+json', 'user-agent': 'nexvr-admin' },
    });
    if (!res.ok) throw new Error(`GitHub responded ${res.status}`);

    const releases = await res.json();
    const list = (Array.isArray(releases) ? releases : []).slice(0, 10).map((rel) => ({
      tag: rel.tag_name,
      published: rel.published_at,
      downloads: (rel.assets || []).reduce((n, a) => n + (a.download_count || 0), 0),
    }));

    const out = {
      available: true,
      total: list.reduce((n, r) => n + r.downloads, 0),
      releases: list,
    };
    if (env.WAITLIST) await env.WAITLIST.put(CACHE_KEY, JSON.stringify(out), { expirationTtl: 3600 });
    return out;
  } catch (err) {
    console.error('github release fetch failed:', err?.message || err);
    return { available: false, total: 0, releases: [] };
  }
}
