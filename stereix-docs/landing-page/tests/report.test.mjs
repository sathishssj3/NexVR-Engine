import assert from 'node:assert/strict';
import test from 'node:test';
import { onRequestGet, onRequestOptions, onRequestPost } from '../functions/api/report.js';

const URL = 'https://example.test/api/report';
const primaryKey = 'local-primary-key';
const alternateKey = 'local-alternate-key';

function mockKv() {
  const values = new Map([
    ['REPORTS_INDEX', JSON.stringify([{ id: 'rep_123', gameName: 'Test Game' }])],
    ['REPORT:rep_123', JSON.stringify({ id: 'rep_123', logSnippet: 'private log' })],
  ]);
  const gets = [];
  const puts = [];
  return {
    values,
    gets,
    puts,
    async get(key) {
      gets.push(key);
      return values.get(key) ?? null;
    },
    async put(key, value, options) {
      puts.push({ key, options });
      values.set(key, value);
    },
  };
}

function getRequest(query = '', headers = {}) {
  return new Request(`${URL}${query}`, { headers });
}

function postRequest(body, headers = {}) {
  return new Request(URL, {
    method: 'POST',
    headers: { 'content-type': 'application/json', ...headers },
    body: typeof body === 'string' ? body : JSON.stringify(body),
  });
}

function mockFetch(t) {
  const calls = [];
  t.mock.method(globalThis, 'fetch', async (...args) => {
    calls.push(args);
    return new Response(null, { status: 204 });
  });
  return calls;
}

test('GET never reads the index or details without a configured admin key', async () => {
  const kv = mockKv();
  const formerFallback = ['nexvr', 'admin', 'telemetry', 'secret', '2026'].join('_');
  for (const query of ['', '?id=rep_123']) {
    for (const headers of [
      {},
      { authorization: `Bearer ${formerFallback}` },
      { 'x-admin-key': formerFallback },
    ]) {
      const response = await onRequestGet({ request: getRequest(query, headers), env: { WAITLIST: kv } });
      assert.equal(response.status, 401);
      assert.equal(response.headers.get('cache-control'), 'no-store');
    }
  }
  assert.deepEqual(kv.gets, []);
});

test('GET rejects invalid and blank credentials before touching KV', async () => {
  const kv = mockKv();
  const env = { WAITLIST: kv, REPORTS_ADMIN_KEY: primaryKey, ADMIN_API_KEY: alternateKey };
  for (const query of ['', '?id=rep_123']) {
    for (const headers of [
      {},
      { authorization: 'Bearer wrong' },
      { 'x-admin-key': 'wrong' },
      { authorization: 'Bearer ' },
    ]) {
      const response = await onRequestGet({ request: getRequest(query, headers), env });
      assert.equal(response.status, 401);
    }
  }
  const blankResponse = await onRequestGet({
    request: getRequest('', { 'x-admin-key': '   ' }),
    env: { WAITLIST: kv, REPORTS_ADMIN_KEY: '   ', ADMIN_API_KEY: '' },
  });
  assert.equal(blankResponse.status, 401);
  assert.deepEqual(kv.gets, []);
});

test('GET accepts each configured key with Bearer or x-admin-key for index and details', async () => {
  const kv = mockKv();
  const env = { WAITLIST: kv, REPORTS_ADMIN_KEY: primaryKey, ADMIN_API_KEY: alternateKey };
  for (const headers of [
    { authorization: `Bearer ${primaryKey}` },
    { authorization: `Bearer ${alternateKey}` },
    { 'x-admin-key': primaryKey },
    { 'x-admin-key': alternateKey },
  ]) {
    const index = await onRequestGet({ request: getRequest('', headers), env });
    assert.equal(index.status, 200);
    assert.deepEqual(await index.json(), { count: 1, reports: [{ id: 'rep_123', gameName: 'Test Game' }] });

    const details = await onRequestGet({ request: getRequest('?id=rep_123', headers), env });
    assert.equal(details.status, 200);
    assert.deepEqual(await details.json(), { id: 'rep_123', logSnippet: 'private log' });
  }
  const legacyEnv = { WAITLIST: kv, ADMIN_API_KEY: alternateKey };
  const legacyResponse = await onRequestGet({
    request: getRequest('', { authorization: `Bearer ${alternateKey}` }),
    env: legacyEnv,
  });
  assert.equal(legacyResponse.status, 200);
});

test('GET checks credentials before returning storage configuration errors', async () => {
  const unauthorized = await onRequestGet({ request: getRequest(), env: { REPORTS_ADMIN_KEY: primaryKey } });
  assert.equal(unauthorized.status, 401);
  const authorized = await onRequestGet({
    request: getRequest('', { authorization: `Bearer ${primaryKey}` }),
    env: { REPORTS_ADMIN_KEY: primaryKey },
  });
  assert.equal(authorized.status, 503);
});

test('POST stores a launcher-style report without auth and ignores client webhook URLs', async t => {
  const calls = mockFetch(t);
  const kv = mockKv();
  const response = await onRequestPost({
    request: postRequest({
      gameId: 'game-1',
      gameName: 'Test Game',
      engineVersion: '0.1.90',
      status: 'manual_report',
      message: 'A bug occurred',
      userNote: 'A bug occurred',
      logContent: '🎮'.repeat(25000),
      discordWebhookUrl: 'https://discord.com/api/webhooks/client/override',
    }),
    env: { WAITLIST: kv },
  });
  assert.equal(response.status, 200);
  const result = await response.json();
  assert.equal(result.success, true);
  const saved = JSON.parse(kv.values.get(`REPORT:${result.reportId}`));
  assert.equal(saved.gameId, 'game-1');
  assert.equal(saved.userNote, 'A bug occurred');
  assert.equal(saved.logSnippet, '🎮'.repeat(10000));
  assert.equal(kv.puts.length, 2);
  assert.deepEqual(kv.puts.map(put => put.options.expirationTtl), [2592000, 2592000]);
  assert.equal(calls.length, 0);

  const preflight = await onRequestOptions();
  assert.equal(preflight.status, 204);
  assert.match(preflight.headers.get('access-control-allow-methods'), /POST/);
});

test('POST sends notifications only to the configured Discord webhook', async t => {
  const calls = mockFetch(t);
  const configuredUrl = 'https://discord.com/api/webhooks/configured/only';
  const response = await onRequestPost({
    request: postRequest({ gameId: 'game-1', discordWebhookUrl: 'https://discord.com/api/webhooks/client/override' }),
    env: { WAITLIST: mockKv(), DISCORD_WEBHOOK_URL: configuredUrl },
  });
  assert.equal(response.status, 200);
  assert.equal(calls.length, 1);
  assert.equal(calls[0][0], configuredUrl);
});

test('POST rejects oversized declared and streamed bodies before storage or forwarding', async t => {
  const calls = mockFetch(t);
  const kv = mockKv();
  const env = { WAITLIST: kv, DISCORD_WEBHOOK_URL: 'https://discord.com/api/webhooks/configured/only' };
  const declared = await onRequestPost({
    request: postRequest('{}', { 'content-length': String(256 * 1024 + 1) }),
    env,
  });
  assert.equal(declared.status, 413);

  const stream = new ReadableStream({
    start(controller) {
      controller.enqueue(new TextEncoder().encode('{"logContent":"'));
      controller.enqueue(new TextEncoder().encode('x'.repeat(256 * 1024)));
      controller.enqueue(new TextEncoder().encode('"}'));
      controller.close();
    },
  });
  const streamed = await onRequestPost({
    request: new Request(URL, {
      method: 'POST',
      headers: { 'content-length': '1' },
      body: stream,
      duplex: 'half',
    }),
    env,
  });
  assert.equal(streamed.status, 413);
  assert.deepEqual(kv.puts, []);
  assert.equal(calls.length, 0);
});

test('POST rejects malformed or non-object JSON without a KV write', async t => {
  const calls = mockFetch(t);
  const kv = mockKv();
  for (const body of ['{', 'null', '[]']) {
    const response = await onRequestPost({ request: postRequest(body), env: { WAITLIST: kv } });
    assert.equal(response.status, 400);
  }
  assert.deepEqual(kv.puts, []);
  assert.equal(calls.length, 0);
});
