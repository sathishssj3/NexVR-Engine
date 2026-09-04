/**
 * POST /api/waitlist — Cloudflare Pages Function.
 *
 * Stores waitlist signups in KV. Runs on the same origin as the page, so no
 * CORS headers are set: without them no other site can read a response, which
 * is the behaviour we want.
 *
 * Requires a KV namespace bound as WAITLIST. See ../../README.md.
 *
 * A single onRequest handles every method and rejects non-POST itself. Relying
 * on Pages to answer 405 does not work: it falls through to the static asset
 * and serves index.html with a 200, which is a confusing thing for an API to
 * do. There is no read route by design — the address list is never reachable
 * over HTTP. Read it with `wrangler kv key list` instead.
 */

const EMAIL_RE = /^[^@\s]+@[^@\s]+\.[a-z]{2,}$/i;
const MAX_BODY = 1024;      // bytes; a JSON body with one email is ~40
const MAX_EMAIL = 254;      // RFC 5321
const RATE_LIMIT = 5;       // signups per IP per hour

const json = (body, status = 200) =>
  new Response(JSON.stringify(body), {
    status,
    headers: { 'content-type': 'application/json', 'cache-control': 'no-store' },
  });

export async function onRequest({ request, env }) {
  if (request.method !== 'POST') {
    return new Response(JSON.stringify({ error: 'Method not allowed.' }), {
      status: 405,
      headers: { 'content-type': 'application/json', allow: 'POST' },
    });
  }

  if (!env.WAITLIST) {
    // Binding missing — a deploy problem, not the visitor's fault. Say so
    // plainly in the log and stay vague to the client.
    console.error('KV namespace WAITLIST is not bound to this deployment.');
    return json({ error: 'The waitlist is offline right now. Try again later.' }, 503);
  }

  if (Number(request.headers.get('content-length') || 0) > MAX_BODY) {
    return json({ error: 'Request too large.' }, 413);
  }

  let body;
  try {
    body = await request.json();
  } catch {
    return json({ error: 'Malformed request.' }, 400);
  }

  // Honeypot. A real person never sees this field, so anything in it is a bot.
  // Answer exactly like the success path so scripts learn nothing.
  if (body.company) return json({ ok: true });

  const email = String(body.email ?? '').trim().toLowerCase();
  if (email.length > MAX_EMAIL || !EMAIL_RE.test(email)) {
    return json({ error: 'That address does not look right.' }, 400);
  }

  // Per-IP throttle. Cheap, and enough to stop someone scripting the form.
  // ponytail: KV counter, swap for Durable Objects if this ever matters.
  const ip = request.headers.get('cf-connecting-ip') || 'unknown';
  const rateKey = `rate:${ip}`;
  const hits = Number((await env.WAITLIST.get(rateKey)) || 0);
  if (hits >= RATE_LIMIT) {
    return json({ error: 'Too many signups from this connection. Try later.' }, 429);
  }
  await env.WAITLIST.put(rateKey, String(hits + 1), { expirationTtl: 3600 });

  // Signing up twice is not an error — keep the first timestamp.
  const key = `email:${email}`;
  if (!(await env.WAITLIST.get(key))) {
    await env.WAITLIST.put(
      key,
      JSON.stringify({ at: new Date().toISOString(), country: request.cf?.country ?? null })
    );
  }

  return json({ ok: true });
}
