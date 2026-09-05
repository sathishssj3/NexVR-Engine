/**
 * GET /api/dl?to=installer — counted download redirect.
 *
 * Every download button on the page points here instead of straight at GitHub.
 * A click handler would have been simpler and would also have been wrong: it
 * misses anyone running an ad blocker, anyone with JS off, and every
 * middle-click, "open in new tab" and "copy link address". A 302 counts all of
 * them, and the visitor sees nothing but the download they asked for.
 *
 * The destination is looked up in a table here and never read from the query
 * string. Taking a URL from the caller would make this an open redirect — a
 * link that looks like it points at this domain but lands anywhere, which is
 * exactly the shape phishing wants.
 */

import { record } from '../_lib/analytics.js';

const REPO = 'https://github.com/sathishssj3/NexVR-Engine-Releases';

const TARGETS = {
  installer: `${REPO}/releases/latest/download/NexVR-Engine-Setup.exe`,
  releases: `${REPO}/releases`,
  repo: REPO,
};

export async function onRequest({ request, env, waitUntil }) {
  const to = new URL(request.url).searchParams.get('to') || 'installer';
  const dest = TARGETS[to];

  if (!dest) {
    return new Response('Unknown download target.', {
      status: 400,
      headers: { 'content-type': 'text/plain', 'cache-control': 'no-store' },
    });
  }

  // Log after the redirect is already on its way — the visitor never waits for
  // a database write to start their download.
  const write = record(env, request, [
    { type: 'download', path: '/api/dl', ref: request.headers.get('referer'), meta: { to } },
  ]);
  if (typeof waitUntil === 'function') waitUntil(write); else await write;

  return new Response(null, {
    status: 302,
    headers: {
      location: dest,
      'cache-control': 'no-store, no-cache, must-revalidate',
      // Don't leak the landing page URL onward to GitHub.
      'referrer-policy': 'no-referrer',
    },
  });
}
