CREATE TABLE native_contact_links (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    local_uid TEXT NOT NULL,
    backend TEXT NOT NULL,            -- 'akonadi' | 'eds'
    native_item_id TEXT NOT NULL,
    native_source_id TEXT NOT NULL,
    last_synced_hash TEXT NOT NULL,   -- sha256 of the agreed vCard text at last sync
    last_synced_at TEXT NOT NULL,
    UNIQUE(local_uid, backend),
    UNIQUE(backend, native_item_id)
);
