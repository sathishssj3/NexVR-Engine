#!/usr/bin/env node
/**
 * fetch_reports.mjs — Telemetry & Bug Report Fetcher for NexVR Engine
 *
 * Usage:
 *   node scripts/fetch_reports.mjs              # Lists recent reports
 *   node scripts/fetch_reports.mjs <report_id>  # Fetches details & logs for a specific report
 *   node scripts/fetch_reports.mjs --save       # Syncs reports to docs/incoming_bugs.json
 */

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const REPO_ROOT = path.resolve(__dirname, '..');

const API_ENDPOINT = process.env.NEXVR_TELEMETRY_API || 'https://nexvr-engine.pages.dev/api/report';

async function main() {
  const args = process.argv.slice(2);
  const isSave = args.includes('--save');
  const isJson = args.includes('--json');
  const targetId = args.find(a => !a.startsWith('--') && (a.startsWith('rep_') || a.length > 5));

  if (targetId) {
    console.log(`\x1b[36m[NexVR]\x1b[0m Fetching report details for: ${targetId}...`);
    try {
      const res = await fetch(`${API_ENDPOINT}?id=${encodeURIComponent(targetId)}`);
      if (!res.ok) {
        console.error(`\x1b[31m[Error]\x1b[0m Failed to fetch report ${targetId} (HTTP ${res.status})`);
        process.exit(1);
      }
      const data = await res.json();
      if (isJson) {
        console.log(JSON.stringify(data, null, 2));
        return;
      }

      console.log('\n' + '='.repeat(70));
      console.log(`\x1b[1m\x1b[35m📋 NEXVR BUG REPORT DETAILS [${data.id}]\x1b[0m`);
      console.log('='.repeat(70));
      console.log(`\x1b[33mGame:\x1b[0m           ${data.gameName} (${data.gameId})`);
      console.log(`\x1b[33mDate / Time:\x1b[0m    ${data.date}`);
      console.log(`\x1b[33mVersion:\x1b[0m        v${data.engineVersion}`);
      console.log(`\x1b[33mStatus:\x1b[0m         ${data.status}`);
      console.log(`\x1b[33mVR Headset:\x1b[0m     ${data.vrHeadset}`);
      console.log(`\x1b[33mVR Runtime:\x1b[0m     ${data.vrRuntime}`);
      console.log(`\x1b[33mCountry:\x1b[0m        ${data.country}`);
      if (data.userNote) {
        console.log(`\x1b[32mTester Note:\x1b[0m    ${data.userNote}`);
      }
      if (data.message && data.message !== data.userNote) {
        console.log(`\x1b[32mMessage:\x1b[0m        ${data.message}`);
      }
      if (data.logSnippet) {
        console.log('\n' + '-'.repeat(70));
        console.log(`\x1b[1m\x1b[36mENGINE LOG SNIPPET (Last lines):\x1b[0m`);
        console.log('-'.repeat(70));
        const snippet = data.logSnippet.trim();
        const lines = snippet.split('\n');
        const displayLines = lines.length > 30 ? lines.slice(-30).join('\n') : snippet;
        console.log(displayLines);
      }
      console.log('='.repeat(70) + '\n');
    } catch (err) {
      console.error(`\x1b[31m[Error]\x1b[0m ${err.message}`);
      process.exit(1);
    }
    return;
  }

  // Fetch Index
  console.log(`\x1b[36m[NexVR]\x1b[0m Fetching recent beta telemetry reports from Cloudflare...`);
  try {
    const res = await fetch(API_ENDPOINT);
    if (!res.ok) {
      console.error(`\x1b[31m[Error]\x1b[0m HTTP ${res.status}: ${res.statusText}`);
      process.exit(1);
    }
    const data = await res.json();
    const reports = data.reports || [];

    if (isJson) {
      console.log(JSON.stringify(reports, null, 2));
      return;
    }

    if (reports.length === 0) {
      console.log('\x1b[32m✓ No bug reports recorded yet. Systems nominal.\x1b[0m');
      return;
    }

    console.log(`\nFound \x1b[1m${reports.length}\x1b[0m report(s):\n`);
    console.log(
      'ID'.padEnd(28) +
      'DATE (UTC)'.padEnd(22) +
      'GAME'.padEnd(24) +
      'VER'.padEnd(10) +
      'HEADSET'.padEnd(18) +
      'NOTE'
    );
    console.log('-'.repeat(110));

    for (const r of reports) {
      const id = String(r.id || '').padEnd(28);
      const date = String(r.date || '').slice(0, 19).replace('T', ' ').padEnd(22);
      const game = String(r.gameName || '').slice(0, 22).padEnd(24);
      const ver = `v${r.engineVersion || '0.1.90'}`.padEnd(10);
      const hmd = String(r.vrHeadset || 'Unknown').slice(0, 16).padEnd(18);
      const note = String(r.userNote || '').slice(0, 35);
      console.log(`${id}${date}${game}${ver}${hmd}${note}`);
    }
    console.log('-'.repeat(110));
    console.log(`\nTip: To view details and logs for a report, run: node scripts/fetch_reports.mjs <ID>\n`);

    if (isSave) {
      const outPath = path.join(REPO_ROOT, 'docs', 'incoming_bugs.json');
      fs.writeFileSync(outPath, JSON.stringify(reports, null, 2), 'utf-8');
      console.log(`\x1b[32m✓ Saved ${reports.length} report(s) to docs/incoming_bugs.json\x1b[0m`);
    }
  } catch (err) {
    console.error(`\x1b[31m[Error]\x1b[0m ${err.message}`);
    process.exit(1);
  }
}

main();
