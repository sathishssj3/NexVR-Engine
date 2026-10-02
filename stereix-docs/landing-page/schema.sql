-- NexVR Engine — analytics schema (Cloudflare D1 / SQLite)
--
-- Apply it with:
--   wrangler d1 execute nexvr-analytics --remote --file=./schema.sql
--   wrangler d1 execute nexvr-analytics --local  --file=./schema.sql   (for pages dev)
--
-- One row per event. There is deliberately no visitor table and no cookie:
-- `visitor` is a salted hash that rotates every day, so daily uniques are
-- countable and nothing else is — it cannot be reversed to an address and it
-- cannot link the same person across two days.

CREATE TABLE IF NOT EXISTS events (
  id       INTEGER PRIMARY KEY AUTOINCREMENT,
  ts       INTEGER NOT NULL,              -- unix seconds, UTC
  day      TEXT    NOT NULL,              -- 'YYYY-MM-DD', UTC. Denormalised so
                                          -- every grouping is an index hit.
  type     TEXT    NOT NULL,              -- view | download | waitlist | section | dwell
  path     TEXT    NOT NULL DEFAULT '/',
  ref_host TEXT,                          -- referrer HOSTNAME only, never the
                                          -- full URL (query strings carry PII)
  country  TEXT,                          -- ISO-3166 alpha-2, from Cloudflare
  device   TEXT,                          -- desktop | mobile | tablet
  visitor  TEXT NOT NULL,                 -- daily-rotating salted hash
  meta     TEXT                           -- small JSON: {"to":"installer"} etc.
);

CREATE INDEX IF NOT EXISTS idx_events_day      ON events(day);
CREATE INDEX IF NOT EXISTS idx_events_type_day ON events(type, day);
CREATE INDEX IF NOT EXISTS idx_events_visitor  ON events(day, visitor);
CREATE INDEX IF NOT EXISTS idx_events_ref      ON events(ref_host) WHERE ref_host IS NOT NULL;
