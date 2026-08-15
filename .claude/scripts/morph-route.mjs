#!/usr/bin/env node
// Morph Router -> Claude Code UserPromptSubmit advisory hook.
//
// Asks Morph's router which of the allowed Claude models is the cheapest one
// that can handle this prompt, and prints a single advisory line into context.
//
// ADVISORY ONLY. Claude Code's model is chosen by /model or settings; a hook
// cannot change it mid-session. This exists so the suggestion is visible at the
// moment it matters, instead of being a judgement call made by hand every turn.
//
// FAIL-OPEN BY DESIGN: no API key, a network error, a bad response, or anything
// slower than TIMEOUT_MS all result in printing nothing and exiting 0. A router
// outage must never block or delay a prompt.
//
// Manual test:
//   node .claude/scripts/morph-route.mjs "fix a typo in the README"

const ENDPOINT = 'https://api.morphllm.com/v1/router/multimodel';
// Docs claim ~180ms. Measured against the live endpoint: 326-477ms warm,
// 991ms cold. 1200 covers a cold call; anything slower is not worth blocking
// a prompt for, so it lapses into the silent fail-open path.
const TIMEOUT_MS = 1200;

// Restrict the router to models this harness can actually run. Morph's catalog
// spans many providers; recommending gpt-5.5 here would be noise.
const ALLOWED_MODELS = [
  'claude-opus-5',
  'claude-sonnet-5',
  'claude-haiku-4-5-20251001',
];

function readStdin() {
  return new Promise((resolve) => {
    if (process.stdin.isTTY) return resolve('');
    let data = '';
    process.stdin.setEncoding('utf8');
    process.stdin.on('data', (chunk) => { data += chunk; });
    // pause() releases the libuv handle. Without it, forcing exit while the
    // stream is mid-close aborts with
    // "Assertion failed: !(handle->flags & UV_HANDLE_CLOSING)".
    process.stdin.on('end', () => { process.stdin.pause(); resolve(data); });
    process.stdin.on('error', () => { process.stdin.pause(); resolve(''); });
  });
}

// The hook delivers a JSON payload on stdin; argv is the manual-test path.
function parseStdinPrompt(stdin) {
  if (!stdin.trim()) return '';
  try {
    const payload = JSON.parse(stdin);
    return (payload.prompt ?? payload.user_prompt ?? payload.message ?? '').trim();
  } catch {
    return stdin.trim();   // not JSON - treat the raw text as the prompt
  }
}

// Shorten "claude-haiku-4-5-20251001" to "haiku-4-5" so the advisory line stays
// scannable. Unknown ids pass through untouched.
function shorten(model) {
  if (typeof model !== 'string') return String(model ?? '');
  const m = model.match(/^claude-([a-z]+(?:-[\d.]+)*)/i);
  return m ? m[1] : model;
}

async function main() {
  const apiKey = process.env.MORPH_API_KEY;
  if (!apiKey) return;                        // not configured - stay silent

  // Only touch stdin when argv is empty - attaching stdin handlers we do not
  // need is what provoked the libuv close assertion.
  const fromArgv = process.argv.slice(2).join(' ').trim();
  const prompt = fromArgv || parseStdinPrompt(await readStdin());
  if (!prompt) return;

  const response = await fetch(ENDPOINT, {
    method: 'POST',
    headers: {
      'Authorization': `Bearer ${apiKey}`,
      'Content-Type': 'application/json',
    },
    // Field is "input", not "prompt". Sending "prompt" returns 400 with an
    // empty body, which the fail-open path below swallows into silence.
    body: JSON.stringify({
      input: prompt,
      allowed_models: ALLOWED_MODELS,
      // "balanced", not "cost_efficient": cost_efficient returned haiku for
      // both a typo fix and a 6DOF matrix derivation, so it never surfaced the
      // one recommendation worth having - escalate to opus for hard work.
      policy: 'balanced',
    }),
    signal: AbortSignal.timeout(TIMEOUT_MS),
  });

  if (!response.ok) return;                   // 4xx/5xx - stay silent
  const result = await response.json();

  const model = result.model ?? result.selected_model ?? result.recommendation;
  if (!model) return;

  const bits = [];
  if (result.difficulty) bits.push(result.difficulty);
  if (result.domain) bits.push(result.domain);
  const detail = bits.length ? ` (${bits.join('/')})` : '';

  process.stdout.write(`Morph Router suggests: ${shorten(model)}${detail}\n`);
}

// Single catch-all: every failure path is silent success. Never throw, never
// print to stderr - stderr from a hook is surfaced to the user as noise.
// Let the event loop drain on its own; process.exit() here raced the stdin
// handle teardown and aborted the process.
main().catch(() => { process.exitCode = 0; });
