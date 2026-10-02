/**
 * POST /api/event — analytics beacon.
 *
 * The landing page sends two of these: one `view` the moment the page loads,
 * and one batched flush when the tab is hidden carrying the sections that were
 * actually read plus how long the visit lasted.
 *
 * Always answers 204 with an empty body. A beacon has nothing to say back, and
 * a body would only be something for a script to probe. Bad input is dropped
 * silently for the same reason — this endpoint is public and unauthenticated,
 * so it should be as uninteresting as possible to poke at.
 */

import { record, json } from '../_lib/analytics.js';

const MAX_BODY = 2048;
const ALLOWED = new Set(['view', 'section', 'dwell']);
const NO_CONTENT = () => new Response(null, { status: 204, headers: { 'cache-control': 'no-store' } });

export async function onRequest({ request, env }) {
  if (request.method !== 'POST') {
    return json({ error: 'Method not allowed.' }, 405, { allow: 'POST' });
  }
  if (Number(request.headers.get('content-length') || 0) > MAX_BODY) return NO_CONTENT();

  let body;
  try {
    body = await request.json();
  } catch {
    return NO_CONTENT();
  }

  const path = String(body.path || '/').slice(0, 200);
  const ref = typeof body.ref === 'string' ? body.ref : null;
  const rows = [];

  if (ALLOWED.has(body.type)) {
    rows.push({ type: body.type, path, ref, meta: cleanMeta(body.type, body.meta) });
  }

  // Batched flush: {sections:[ids], dwell: seconds}
  if (Array.isArray(body.sections)) {
    for (const id of body.sections.slice(0, 24)) {
      if (typeof id === 'string' && id.length && id.length < 40) {
        rows.push({ type: 'section', path, ref, meta: { id } });
      }
    }
  }
  const dwell = Number(body.dwell);
  if (Number.isFinite(dwell) && dwell > 0) {
    // Cap it: a tab left open overnight is not a five-hour reading session.
    rows.push({ type: 'dwell', path, ref, meta: { s: Math.min(Math.round(dwell), 1800) } });
  }

  if (rows.length) await record(env, request, rows);
  return NO_CONTENT();
}

function cleanMeta(type, meta) {
  if (!meta || typeof meta !== 'object') return null;
  if (type === 'section' && typeof meta.id === 'string') return { id: meta.id.slice(0, 40) };
  return null;
}
