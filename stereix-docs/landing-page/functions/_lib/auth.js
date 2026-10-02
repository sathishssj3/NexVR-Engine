/**
 * Admin session auth.
 *
 * One password, held as a Cloudflare secret, exchanged for an HMAC-signed
 * session cookie. There is no user table and no server-side session store: the
 * cookie carries its own expiry and its own signature, so verifying it is a
 * single HMAC with no I/O.
 *
 *   wrangler pages secret put ADMIN_PASSWORD
 *
 * Optionally also set ADMIN_SESSION_SECRET. If you don't, the signing key is
 * derived from the password — which has the useful property that changing the
 * password immediately invalidates every session that was already issued.
 */

const COOKIE = 'nexvr_admin';
const TTL = 60 * 60 * 12;        // 12 hours
const LOGIN_LIMIT = 8;           // attempts per IP per window
const LOGIN_WINDOW = 900;        // 15 minutes

const enc = new TextEncoder();

const b64url = (bytes) =>
  btoa(String.fromCharCode(...new Uint8Array(bytes)))
    .replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');

const unb64url = (s) =>
  Uint8Array.from(atob(s.replace(/-/g, '+').replace(/_/g, '/')), (c) => c.charCodeAt(0));

const signingKey = (env) => env.ADMIN_SESSION_SECRET || `sig:${env.ADMIN_PASSWORD}`;

async function hmac(secret, data) {
  const key = await crypto.subtle.importKey(
    'raw', enc.encode(secret), { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']
  );
  return b64url(await crypto.subtle.sign('HMAC', key, enc.encode(data)));
}

/**
 * Compare two strings without letting the clock reveal how much of the guess
 * was right. The loop always runs to the longer of the two, so an attacker
 * learns nothing from response time — including nothing about the length.
 */
export function timingSafeEqual(a, b) {
  const A = enc.encode(String(a));
  const B = enc.encode(String(b));
  let diff = A.length ^ B.length;
  const n = Math.max(A.length, B.length);
  for (let i = 0; i < n; i++) diff |= (A[i] ?? 0) ^ (B[i] ?? 0);
  return diff === 0;
}

export function readCookie(header, name) {
  if (!header) return null;
  for (const part of header.split(';')) {
    const [k, ...v] = part.trim().split('=');
    if (k === name) return v.join('=');
  }
  return null;
}

export async function issueSession(env) {
  const payload = b64url(enc.encode(JSON.stringify({ exp: Math.floor(Date.now() / 1000) + TTL })));
  return `${payload}.${await hmac(signingKey(env), payload)}`;
}

export async function verifySession(env, request) {
  if (!env.ADMIN_PASSWORD) return false;
  const raw = readCookie(request.headers.get('cookie'), COOKIE);
  if (!raw) return false;

  const [payload, sig] = raw.split('.');
  if (!payload || !sig) return false;

  // Signature first: an unsigned payload is never parsed.
  if (!timingSafeEqual(sig, await hmac(signingKey(env), payload))) return false;

  try {
    const { exp } = JSON.parse(new TextDecoder().decode(unb64url(payload)));
    return typeof exp === 'number' && exp > Math.floor(Date.now() / 1000);
  } catch {
    return false;
  }
}

export function sessionCookie(request, value, maxAge = TTL) {
  // Secure would make the cookie unusable over plain http, which is how
  // `wrangler pages dev` serves locally. Set it whenever we are actually on TLS.
  const secure = new URL(request.url).protocol === 'https:' ? ' Secure;' : '';
  // Lax, not Strict. Strict withholds the cookie on any navigation that did not
  // start on this origin, so opening /admin from a bookmark in a fresh tab, or
  // from a link, drops you on the login screen with a perfectly valid session.
  // Lax still refuses to travel on cross-site POSTs and sub-resource requests,
  // which is the part that actually stops CSRF — and every state-changing call
  // here is a same-origin fetch.
  return `${COOKIE}=${value}; Path=/; HttpOnly;${secure} SameSite=Lax; Max-Age=${maxAge}`;
}

export const clearCookie = (request) => sessionCookie(request, '', 0);

/**
 * Throttle login attempts per IP. Uses the KV namespace the waitlist already
 * binds, so this needs no extra infrastructure. Returns true when the caller
 * has spent its budget.
 */
export async function loginThrottled(env, request) {
  if (!env.WAITLIST) return false;                 // no store — fail open, never lock the owner out
  const ip = request.headers.get('cf-connecting-ip') || 'unknown';
  const key = `login:${ip}`;
  const hits = Number((await env.WAITLIST.get(key)) || 0);
  if (hits >= LOGIN_LIMIT) return true;
  await env.WAITLIST.put(key, String(hits + 1), { expirationTtl: LOGIN_WINDOW });
  return false;
}

export async function clearLoginThrottle(env, request) {
  if (!env.WAITLIST) return;
  const ip = request.headers.get('cf-connecting-ip') || 'unknown';
  await env.WAITLIST.delete(`login:${ip}`);
}
