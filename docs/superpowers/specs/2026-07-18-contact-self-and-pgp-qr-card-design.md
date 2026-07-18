# Contact self-flag + PGP QR contact card — design

## Problem

kypost-server recently shipped a feature (see its own
`docs/superpowers/specs/2026-07-15-pgp-qr-contact-card-design.md`): a
per-contact `isSelf` flag, a toggle endpoint to set it, and a `contactCard`
object folded into the PGP QR key-exchange response. This Linux client has
no awareness of any of it:

- `core/models/Contact.h` has no `isSelf` field (nor `mergedUIDs`/
  `mergedInto`, also new on the server for dedupe provenance).
- `core/net/ContactSyncClient.cpp`'s `ContactWire::contactFromJson` silently
  drops `isSelf`/`mergedUIDs`/`mergedInto` from every synced contact, even
  though the server already includes them in `/api/contacts/sync` responses
  today.
- `core/net/PgpQrClient.cpp`'s `fetchKey()` only reads `name`/`fingerprint`/
  `publicKey` from the scan response and ignores `contactCard` entirely.

This causes exactly the symptoms reported: the user's own contact isn't
visibly marked or sorted to the top, and scanning someone's PGP QR code
never surfaces their shared contact details.

## Scope decision: read-only `isSelf` on this client

`POST /api/contacts/{id}/self` (the only way to *set* the flag) is
registered on the server with `s.withAuth` — cookie-session auth only. Every
endpoint this client actually calls (`/api/contacts/sync`, `/api/contacts/
{id}/photo` GET, `/api/pgp/qr/token`) uses `s.withMailAuth` instead
(device-pairing `sub`/`hash` query params); this client has no
session-cookie/login mechanism at all. Separately, the server's
`Store.Upsert` unconditionally copies `IsSelf` from the *existing* record on
every update (`c.IsSelf = existing.IsSelf`), so pushing `isSelf` through the
normal sync-push path is a no-op by server design even if this client sent
it.

Given that, this plan makes the client **read-only** for `isSelf`: it
displays and sorts on the flag it receives over sync, with no "mark as my
contact" button. A user sets the flag once via the kypost-server web
frontend; it then shows correctly here. No kypost-server changes are in
scope for this plan.

## Data model — `core/models/Contact.h`

Two additions, both plain (non-optional) to match the server's Go types and
the client's existing `deleted` field convention:

```cpp
bool isSelf = false;
QVector<QString> mergedUIDs;
std::optional<QString> mergedInto;
```

`mergedUIDs`/`mergedInto` are dedupe-survivor provenance, never shown in any
UI here — they're added purely so the client stops silently destroying them.
Today, since these fields don't exist on the client's `Contact` at all, any
edit-and-push of a dedupe-survivor contact from this client already wipes
its merge provenance server-side: `Store.Upsert` only special-cases
`CreatedAt` and `IsSelf` on update, so `MergedUIDs`/`MergedInto` get
silently zeroed by whatever the client's JSON omits them as. Adding the
fields and round-tripping them unchanged (same "pass through, never invent"
rule already used for `photoRef`) fixes that pre-existing data-loss bug as a
side effect.

## Wire format — `core/net/ContactSyncClient.cpp`

`isSelf` follows the existing `deleted` bool convention (omit when false,
read via plain `.toBool()`). `mergedUIDs` follows the existing `groupIDs`
convention (always-emit array, via `stringListToJson`/`stringListFromJson`).
`mergedInto` follows the existing optional-string convention (`putOptional`/
`takeOptional`, same as every other `std::optional<QString>` field).

## SQLite — `core/db/migrations/004_contact_self_and_merged.sql`

Three new columns on `contacts`, picked up automatically by the existing
`file(GLOB CONFIGURE_DEPENDS db/migrations/*.sql)` + generated
`MigrationSql.h` mechanism (no CMakeLists changes needed):

```sql
ALTER TABLE contacts ADD COLUMN is_self INTEGER DEFAULT 0;
ALTER TABLE contacts ADD COLUMN merged_uids_json TEXT DEFAULT '[]';
ALTER TABLE contacts ADD COLUMN merged_into TEXT DEFAULT NULL;
```

## Display — sort-to-top + badge

`ContactsController::load()` partitions the self-flagged contact (if any)
to the front of the vector before calling `ContactListModel::setContacts()`
— the model itself stays a passive display layer, matching its own existing
"read-only wrapper, `ContactsController` is the only writer" contract.
`ContactListModel` gains an `IsSelfRole`; `ContactsList.qml`/`ContactDetail.
qml` show a small "You" badge when it's set. No edit-form field, no toggle
button, per the scope decision above.

## PGP QR contact card — `core/net/PgpQrClient.cpp` / `PgpQrController`

`pgpQRContactCard`'s fields on the wire are a strict subset of `Contact`'s
own fields (`fn`, `org`, `emails`, `ims`, etc.) with no field-name
translation — so `PgpQrKeyResult` gains `std::optional<Contact> contactCard`
populated by calling the *existing, already-tested*
`ContactWire::contactFromJson` on the `contactCard` object, rather than
writing a second parser. Fields the card never carries (`uid`, `rev`,
`photoRef`, `isSelf`, ...) simply stay default-valued on the resulting
`Contact` and are never read.

`PgpQrController` exposes `scannedContactCardFields(): QVariantMap`, shaped
to exactly the keys `ContactsController::createContact` already accepts
(`org`, `notes`, `email`, `phone`, `department`, `pronouns`,
`phoneticGivenName`, `phoneticFamilyName`, `ims`, `websites`, `relations`,
`events`, `customFields`) — the single-email/single-phone convention matches
this client's existing create/edit form exactly, so no form changes are
needed. `birthday`/`addresses` are omitted: the create/edit form doesn't
expose those fields today either (a pre-existing gap, out of scope here).

Only the "scan to create a brand-new contact" entry points
(`MobileRoot.qml`/`DesktopRoot.qml`'s `onKeyScanned` when
`targetContactDetail` is null) merge these fields in. The "scan to refresh
an existing contact's key" entry point (`ContactDetail.qml`'s
`applyScannedKey`) is left untouched — overwriting fields on a contact the
user is already editing would be surprising, and that path only ever wanted
the key.

## Testing

Every new/changed field gets a C++ round-trip test at its own layer
(`ContactTest`, `ContactSyncClientTest`, `ContactDaoTest`, `DatabaseTest`'s
migration-count assertion, `ContactListModelTest`, `ContactsControllerTest`,
`PgpQrClientTest`, `PgpQrControllerTest`). QML changes (badges, the
`onKeyScanned` merge) have no existing test harness in this repo and are
verified manually, matching this repo's own precedent (see
`docs/superpowers/plans/2026-07-18-html-compose.md`'s Task 4/5 notes).
