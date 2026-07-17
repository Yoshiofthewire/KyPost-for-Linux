-- Task 1 of the extended-contact-fields feature: 12 new columns on
-- `contacts` (groups, photo, PGP key, IMs, websites, relations, events,
-- phonetic names, department, custom fields, pronouns). See
-- .superpowers/sdd/task-1-brief.md's field-by-field mapping table for the
-- authoritative JSON/SQLite/wire-key mapping. Every later task in this
-- feature that needs schema changes (e.g. Task 2's `groups` table) appends
-- to this same file rather than adding a new numbered migration.

ALTER TABLE contacts ADD COLUMN groups_json TEXT DEFAULT '[]';
ALTER TABLE contacts ADD COLUMN photo_ref TEXT DEFAULT NULL;
ALTER TABLE contacts ADD COLUMN pgp_key TEXT DEFAULT NULL;
ALTER TABLE contacts ADD COLUMN ims_json TEXT DEFAULT '[]';
ALTER TABLE contacts ADD COLUMN websites_json TEXT DEFAULT '[]';
ALTER TABLE contacts ADD COLUMN relations_json TEXT DEFAULT '[]';
ALTER TABLE contacts ADD COLUMN events_json TEXT DEFAULT '[]';
ALTER TABLE contacts ADD COLUMN phonetic_given_name TEXT DEFAULT NULL;
ALTER TABLE contacts ADD COLUMN phonetic_family_name TEXT DEFAULT NULL;
ALTER TABLE contacts ADD COLUMN department TEXT DEFAULT NULL;
ALTER TABLE contacts ADD COLUMN custom_fields_json TEXT DEFAULT '[]';
ALTER TABLE contacts ADD COLUMN pronouns TEXT DEFAULT NULL;
