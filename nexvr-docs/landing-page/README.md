# NexVR Engine — landing page

Static page plus one Cloudflare Pages Function. No build step: `index.html` is
served as-is, and `functions/api/waitlist.js` becomes `POST /api/waitlist` on
the same origin.

```text
landing-page/
├── public/                     the ONLY directory published to the web
│   ├── index.html              the landing page — markup, CSS and JS inline
│   ├── admin.html              the admin console, served at /admin
│   ├── logo.png                brand mark, 256px (from assets/logo.png)
│   └── favicon.ico
├── functions/
│   ├── _lib/                   shared code; the underscore keeps it un-routed
│   │   ├── analytics.js        event recording, bot filter, visitor hashing
│   │   └── auth.js             password check, signed session cookie
│   └── api/
│       ├── waitlist.js         POST /api/waitlist
│       ├── event.js            POST /api/event      (analytics beacon)
│       ├── dl.js               GET  /api/dl?to=...  (counted download redirect)
│       └── admin/[[route]].js  /api/admin/*         (login, stats, CSV)
├── schema.sql                  D1 tables
├── wrangler.toml               KV + D1 bindings
└── README.md
```

Nothing outside `public/` is reachable over HTTP. That matters: `wrangler.toml`
carries binding ids and `.dev.vars` carries the real admin password, and with
`pages_build_output_dir = "."` both were being served as static files.

`logo.png` is a downscaled copy of `assets/logo.png` in the repository root.
It has to live inside this folder because Cloudflare Pages only publishes the
build output directory — a path like `../assets/logo.png` would 404 once
deployed. Regenerate it with:

```bash
python -c "from PIL import Image; Image.open('../assets/logo.png').resize((256,256), Image.LANCZOS).save('logo.png', optimize=True)"
```

## Run it locally

```bash
cd landing-page
npx wrangler pages dev . --kv WAITLIST
```

`--kv WAITLIST` gives you a local, in-memory KV namespace, so the waitlist works
without touching your Cloudflare account.

Opening `index.html` directly over `file://` will **not** work — the page uses ES
module imports, which browsers block on that protocol. It needs to be served over
HTTP.

## Deploy

### 1. Create the KV namespace

```bash
npx wrangler kv namespace create WAITLIST
```

Copy the printed `id` into `wrangler.toml`, replacing `PASTE_KV_NAMESPACE_ID_HERE`.

### 2. Create the Pages project

In the Cloudflare dashboard: **Workers & Pages → Create → Pages → Connect to Git**,
pick this repository, and set:

| Setting | Value |
| :--- | :--- |
| Build command | *(leave empty)* |
| Build output directory | `landing-page` |

Cloudflare Pages deploys from a private repository on the free plan, so the engine
source stays closed.

### 3. Bind KV to the project

**Settings → Bindings → Add → KV namespace**, with variable name `WAITLIST`
pointing at the namespace from step 1. Do this for both Production and Preview,
then redeploy — bindings only attach on the next build.

### 4. Custom domain

**Custom domains → Set up a domain.** TLS is issued automatically.

## Reading the waitlist

There is deliberately no HTTP route that returns the list. Read it from the CLI:

```bash
npx wrangler kv key list --binding WAITLIST | grep '"email:'
npx wrangler kv key get --binding WAITLIST "email:someone@example.com"
```

Signups are stored as `email:<address>` (lowercased, deduplicated) with a
timestamp and country code. Throttle counters are stored as `rate:<ip>` and
expire after an hour.

## Before the download button goes live

Two things are still open, and both are outside this folder.

**The binaries are not signed.** `SECURITY.md` requires `vrinject.dll` and
`vr-inject-cli.exe` to be code-signed before public distribution. Unsigned, they
are an injection DLL that hooks other processes — Windows Defender and SmartScreen
will flag them, and a share of downloaders will hit a warning or a silent
quarantine. Sign them, or keep the page on waitlist-only until you have a
certificate.

**The download links point at a repository that does not exist yet.** They expect
a *public* releases repo, so the proprietary engine source can stay private:

```text
https://github.com/sathishssj3/NexVR-Engine-Releases/releases/latest/download/NexVR-Engine-Setup.exe
```

Create `NexVR-Releases` as a public repository, then attach
`NexVR-Engine-Setup.exe` to a release tagged `v0.1.3`. Until that exists, every
download button on the page is a 404. If you pick a different repository name,
update the four `NexVR-Releases` links in `index.html`.

## Unrelated inconsistency worth fixing

`LICENSE` is a proprietary agreement, but `README.md` in the repository root ends
with "This project is licensed under the MIT License." Those cannot both be true,
and the MIT line is the one that would let someone redistribute the engine.


## Analytics and the admin console

`/admin` shows traffic, downloads, waitlist signups and how far people read.
It needs a D1 database and one secret. Run this once:

```bash
# 1. event store
wrangler d1 create nexvr-analytics          # paste database_id into wrangler.toml
wrangler d1 execute nexvr-analytics --remote --file=./schema.sql

# 2. the admin password, and a salt for visitor hashing
wrangler pages secret put ADMIN_PASSWORD
wrangler pages secret put ANALYTICS_SALT    # any long random string

# 3. ship it
wrangler pages deploy
```

For local work, put the same values in `.dev.vars` (gitignored) and use the
local database:

```bash
printf 'ADMIN_PASSWORD="dev"
ANALYTICS_SALT="dev-salt"
' > .dev.vars
npm run db:init      # applies schema.sql to the LOCAL d1
npm run dev
```

### What gets collected

No cookies and no cross-day identifiers. A visitor is
`SHA-256(day | ip | user-agent | salt)` truncated to 32 chars, which counts
daily uniques and rotates at midnight UTC — it cannot be reversed to an address
and cannot link one person across two days. Referrers are reduced to a bare
hostname before storage, because full referrer URLs carry search terms. Raw
events are deleted after 120 days by an opportunistic prune on ~1% of writes.

Known bots are dropped before anything is written. That means analytics numbers
are intentionally lower than raw request counts, and a signup made by something
that looks like a bot still reaches the waitlist while being left out of the
charts.

### Downloads

Every download button points at `/api/dl?to=installer`, which records the click
and then 302s to GitHub. A JavaScript click handler would have missed anyone
running an ad blocker, anyone with JS off, and every middle-click. The
destination is looked up in a table inside `dl.js` and never taken from the
query string — accepting a URL there would turn the endpoint into an open
redirect.

The dashboard shows those clicks next to GitHub's own completed-download counts
(fetched from the public releases API, cached an hour in KV). The two numbers
answer different questions and the gap between them is the interesting part.
