/**
 * Shared analytics helpers.
 *
 * Lives under functions/_lib/ — Cloudflare Pages does not turn files or
 * directories prefixed with an underscore into routes, so nothing here is
 * reachable over HTTP. It is imported by the real endpoints and bundled.
 *
 * Every function here is written to fail soft. Analytics must never be the
 * reason a page or a download breaks: if the D1 binding is missing, or a write
 * throws, the caller carries on and the visitor sees nothing.
 */

const RETAIN_DAYS = 120;   // raw events older than this are dropped
const PRUNE_ODDS  = 0.01;  // ~1 in 100 writes also prunes, so it self-maintains

/* Bots inflate every number they touch and none of them are customers. */
const BOT_RE = /bot|crawl|spider|slurp|bing|yandex|duckduck|baidu|semrush|ahrefs|mj12|dotbot|petal|headless|lighthouse|pagespeed|gtmetrix|curl|wget|python-requests|node-fetch|axios|go-http|java\/|okhttp|monitor|uptime|scanner|preview|embed/i;

export const isBot = (ua) => !ua || BOT_RE.test(ua);

export const today = () => new Date().toISOString().slice(0, 10);

/** 'YYYY-MM-DD' for `n` days before today, UTC. */
export function dayOffset(n) {
  const d = new Date();
  d.setUTCDate(d.getUTCDate() - n);
  return d.toISOString().slice(0, 10);
}

async function sha256hex(str) {
  const buf = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(str));
  return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

/**
 * Pseudonymous visitor id. Salted with the date, so it rotates at midnight UTC:
 * enough to count how many distinct people showed up today, useless for
 * following anyone from one day to the next. Set ANALYTICS_SALT in production —
 * without it the hash is still one-way, but a known salt makes it guessable.
 */
export async function visitorHash(request, env, day) {
  const ip = request.headers.get('cf-connecting-ip') || '';
  const ua = request.headers.get('user-agent') || '';
  const salt = env.ANALYTICS_SALT || 'nexvr-unsalted';
  return (await sha256hex(`${day}|${ip}|${ua}|${salt}`)).slice(0, 32);
}

export function deviceOf(ua = '') {
  if (/ipad|tablet|playbook|silk|kindle/i.test(ua)) return 'tablet';
  if (/mobi|android|iphone|ipod|windows phone/i.test(ua)) return 'mobile';
  return 'desktop';
}

/**
 * Referrer reduced to a bare hostname. A full referrer URL can carry search
 * terms and session ids in its query string, and the only thing worth knowing
 * is which site sent the visitor. Same-origin referrers become null so internal
 * navigation is not counted as a traffic source.
 */
export function refHost(ref, selfHost) {
  if (!ref) return null;
  try {
    const h = new URL(ref).hostname.replace(/^www\./, '');
    const self = (selfHost || '').replace(/^www\./, '');
    if (!h || h === self) return null;
    return h.slice(0, 100);
  } catch {
    return null;
  }
}

/**
 * Append one or more events. Rows are inserted in a single D1 batch so a page
 * reporting six read sections costs one round trip, not six.
 *
 * `rows` — [{ type, path, ref, meta }]
 */
export async function record(env, request, rows) {
  if (!env.ANALYTICS || !rows?.length) return;

  const ua = request.headers.get('user-agent') || '';
  if (isBot(ua)) return;

  try {
    const now = new Date();
    const day = now.toISOString().slice(0, 10);
    const ts = Math.floor(now.getTime() / 1000);
    const visitor = await visitorHash(request, env, day);
    const country = request.cf?.country ?? null;
    const device = deviceOf(ua);
    const selfHost = new URL(request.url).hostname;

    const stmt = env.ANALYTICS.prepare(
      `INSERT INTO events (ts, day, type, path, ref_host, country, device, visitor, meta)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`
    );

    await env.ANALYTICS.batch(
      rows.slice(0, 40).map((r) =>
        stmt.bind(
          ts,
          day,
          String(r.type).slice(0, 24),
          String(r.path || '/').slice(0, 200),
          refHost(r.ref, selfHost),
          country,
          device,
          visitor,
          r.meta ? JSON.stringify(r.meta).slice(0, 500) : null
        )
      )
    );

    if (Math.random() < PRUNE_ODDS) await prune(env);
  } catch (err) {
    // Swallowed on purpose — see the file header. Logged so it is still visible
    // in `wrangler pages deployment tail`.
    console.error('analytics write failed:', err?.message || err);
  }
}

async function prune(env) {
  try {
    const cutoff = Math.floor(Date.now() / 1000) - RETAIN_DAYS * 86400;
    await env.ANALYTICS.prepare('DELETE FROM events WHERE ts < ?').bind(cutoff).run();
  } catch (err) {
    console.error('analytics prune failed:', err?.message || err);
  }
}

export const json = (body, status = 200, headers = {}) =>
  new Response(JSON.stringify(body), {
    status,
    headers: { 'content-type': 'application/json', 'cache-control': 'no-store', ...headers },
  });
