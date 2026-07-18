-- Contact self-flag + dedupe-provenance fields: the server's isSelf
-- (per-user "this is me" flag, read-only on this client -- see
-- docs/superpowers/specs/2026-07-18-contact-self-and-pgp-qr-card-design.md)
-- and mergedUIDs/mergedInto (dedupe survivor provenance, never shown in any
-- UI here, round-tripped only to stop silently wiping it on edit-and-push).

ALTER TABLE contacts ADD COLUMN is_self INTEGER DEFAULT 0;
ALTER TABLE contacts ADD COLUMN merged_uids_json TEXT DEFAULT '[]';
ALTER TABLE contacts ADD COLUMN merged_into TEXT DEFAULT NULL;
