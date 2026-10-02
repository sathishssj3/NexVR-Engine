#!/usr/bin/env node
import { execSync } from 'child_process';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const rootDir = path.resolve(__dirname, '..');

console.log('Fetching waitlist subscribers from Cloudflare KV...');

try {
  const output = execSync('npx wrangler kv key list --binding WAITLIST --remote', {
    cwd: rootDir,
    encoding: 'utf8',
  });

  const keys = JSON.parse(output);
  const subscribers = [];

  for (const item of keys) {
    if (item.name && item.name.startsWith('email:')) {
      const email = item.name.replace(/^email:/, '');
      const signedAt = item.metadata?.at || 'Unknown';
      const country = item.metadata?.country || 'Unknown';
      subscribers.push({ email, signedAt, country });
    }
  }

  // Sort by signup date ascending
  subscribers.sort((a, b) => a.signedAt.localeCompare(b.signedAt));

  console.log(`\nFound ${subscribers.length} customer(s) on the waitlist:\n`);
  console.table(subscribers);

  // Write CSV
  const csvRows = ['Email,Signed Up At (UTC),Country'];
  for (const sub of subscribers) {
    csvRows.push(`"${sub.email}","${sub.signedAt}","${sub.country}"`);
  }
  const csvPath = path.join(rootDir, 'waitlist-subscribers.csv');
  fs.writeFileSync(csvPath, csvRows.join('\n'), 'utf8');

  // Write JSON
  const jsonPath = path.join(rootDir, 'waitlist-subscribers.json');
  fs.writeFileSync(jsonPath, JSON.stringify(subscribers, null, 2), 'utf8');

  console.log(`\nSuccessfully exported:`);
  console.log(`  CSV:  ${csvPath}`);
  console.log(`  JSON: ${jsonPath}`);
  console.log(`\nYou can import the CSV directly into any newsletter/broadcast tool (Resend, Brevo, Mailchimp) or use our release broadcast script.\n`);
} catch (err) {
  console.error('Failed to export waitlist:', err.message);
  process.exit(1);
}
