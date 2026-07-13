CREATE TABLE emails (
    message_id TEXT PRIMARY KEY,
    folder TEXT,
    sender TEXT,
    sent_to TEXT,
    cc TEXT,
    bcc TEXT,
    subject TEXT,
    preview TEXT,
    body TEXT,
    label TEXT,
    keywords_json TEXT,
    status TEXT,
    at_utc TEXT,
    has_attachments INTEGER,
    source_mode TEXT
);

CREATE INDEX idx_emails_folder_atutc ON emails(folder, at_utc);

CREATE TABLE contacts (
    uid TEXT PRIMARY KEY,
    rev INTEGER,
    created_at TEXT,
    updated_at TEXT,
    fn TEXT,
    given_name TEXT,
    family_name TEXT,
    middle_name TEXT,
    prefix TEXT,
    suffix TEXT,
    nickname TEXT,
    org TEXT,
    title TEXT,
    notes TEXT,
    birthday TEXT,
    emails_json TEXT,
    phones_json TEXT,
    addresses_json TEXT
);

CREATE TABLE folders (
    path TEXT PRIMARY KEY,
    parent TEXT,
    deletable INTEGER,
    source_mode TEXT
);

CREATE TABLE pending_contact_changes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    contact_uid TEXT,
    change_json TEXT,
    created_at TEXT
);

CREATE TABLE push_notifications (
    message_id TEXT PRIMARY KEY,
    seq INTEGER,
    received_at TEXT,
    consumed INTEGER
);
