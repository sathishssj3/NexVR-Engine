/**
 * Prints the admin password saved locally by the setup step.
 *
 * This reads a file on THIS machine. It cannot read the deployed secret —
 * Cloudflare secrets are write-only, and nothing in the Worker will ever hand
 * the password back over HTTP. If this file is gone, the password is not
 * recoverable: set a new one instead (see the message below).
 */
const fs = require('fs');
const path = require('path');

const FILE = path.join(__dirname, '..', '_admin-password.txt');

if (!fs.existsSync(FILE)) {
  console.log(`
No saved password file at _admin-password.txt

The deployed password cannot be read back. Set a new one:
  dash.cloudflare.com -> Workers & Pages -> nexvr-engine
  -> Settings -> Variables and secrets -> ADMIN_PASSWORD -> Edit
`);
  process.exit(1);
}

const pw = fs.readFileSync(FILE, 'utf8').trim();
const masked = pw.length > 4 ? pw.slice(0, 2) + '*'.repeat(pw.length - 4) + pw.slice(-2) : '****';
console.log(`
  Admin password:  ${masked} (masked for security)

  Sign in at:      https://stereix-engine.pages.dev/admin
  Please configure ADMIN_PASSWORD securely in Cloudflare Dashboard and delete local credential files.
`);
