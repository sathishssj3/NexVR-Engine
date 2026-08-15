#!/usr/bin/env node
// Morph Compact -> shrink a large text file before an agent reads it.
//
// Compact deletes whole lines and never rewrites them: every surviving line is
// byte-for-byte identical to the input. That property is what makes this safe
// to use on logs and transcripts - the output is a strict subset, so anything
// it shows you is genuinely there.
//
// Intended targets: build logs, workflow journal.jsonl files, and the
// "Output too large ... persisted to file" hook spills, which currently cost
// full price to read or get skipped entirely.
//
// Usage:
//   node .claude/scripts/morph-compact.mjs <file> [query] [--ratio 0.5] [--out <file>]
//
// Prints the compressed text to stdout, and a one-line token report to stderr
// so the saving is visible without polluting the output itself.
//
// This NEVER writes over the source file. --out is required to write anywhere.

import { readFile, writeFile } from 'node:fs/promises';

const ENDPOINT = 'https://api.morphllm.com/v1/compact';
const TIMEOUT_MS = 30_000;   // generous: this is an explicit call, not a hook

function parseArgs(argv) {
  const args = argv.slice(2);
  const opts = { ratio: 0.5, out: null, positional: [] };
  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--ratio') { opts.ratio = Number(args[++i]); continue; }
    if (args[i] === '--out') { opts.out = args[++i]; continue; }
    opts.positional.push(args[i]);
  }
  return opts;
}

function die(message) {
  process.stderr.write(`morph-compact: ${message}\n`);
  process.exit(1);
}

async function main() {
  const apiKey = process.env.MORPH_API_KEY;
  if (!apiKey) die('MORPH_API_KEY is not set');

  const { ratio, out, positional } = parseArgs(process.argv);
  const [path, query] = positional;
  if (!path) die('usage: morph-compact.mjs <file> [query] [--ratio N] [--out <file>]');
  if (!Number.isFinite(ratio) || ratio <= 0 || ratio >= 1) {
    die(`--ratio must be between 0 and 1 exclusive, got ${ratio}`);
  }

  const input = await readFile(path, 'utf8');
  if (!input.trim()) die(`${path} is empty`);

  const body = { input, compression_ratio: ratio };
  if (query) body.query = query;   // steers which lines are worth keeping

  const response = await fetch(ENDPOINT, {
    method: 'POST',
    headers: {
      'Authorization': `Bearer ${apiKey}`,
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(body),
    signal: AbortSignal.timeout(TIMEOUT_MS),
  });

  if (!response.ok) {
    die(`API returned ${response.status} ${response.statusText}: ${await response.text()}`);
  }

  const result = await response.json();
  const output = result.output ?? '';
  const used = result.usage ?? {};

  if (out) {
    await writeFile(out, output, 'utf8');
  } else {
    process.stdout.write(output);
  }

  const before = used.input_tokens;
  const after = used.output_tokens;
  if (Number.isFinite(before) && Number.isFinite(after) && before > 0) {
    const saved = Math.round((1 - after / before) * 100);
    process.stderr.write(`morph-compact: ${before} -> ${after} tokens (${saved}% smaller)\n`);
  }
}

main().catch((error) => die(error?.message ?? String(error)));
