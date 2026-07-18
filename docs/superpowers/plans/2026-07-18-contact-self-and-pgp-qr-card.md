# Contact Self-Flag + PGP QR Contact Card Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make this client aware of two kypost-server features it currently silently drops: the per-contact `isSelf` flag (so the user's own contact is marked and sorted to the top) and the `contactCard` object in the PGP QR key-exchange response (so scanning someone's PGP QR code also creates their contact with the details they've shared) — while fixing a related pre-existing data-loss bug where editing a dedupe-survivor contact from this client silently wipes its `mergedUIDs`/`mergedInto` provenance server-side.

**Architecture:** `isSelf`/`mergedUIDs`/`mergedInto` become plain fields on `core/models/Contact.h`, round-tripped through the existing wire-JSON (`ContactSyncClient.cpp`) and SQLite (`ContactDao.cpp` + a new migration) layers exactly like every other Contact field. `isSelf` is read-only on this client (see the design doc's auth-gap finding) — `ContactsController::load()` sorts the self-flagged contact to the front and `ContactListModel` exposes it as a role for a small badge; there is no toggle UI. `PgpQrClient::fetchKey()` parses the response's `contactCard` object by reusing the existing `ContactWire::contactFromJson` (the card's fields are a strict subset of `Contact`'s), and `PgpQrController` exposes the shareable subset as a `QVariantMap` shaped to match `ContactsController::createContact`'s existing field contract, wired into the "scan to create a new contact" flow only.

**Tech Stack:** Qt6/QML, QtTest/ctest, SQLite (via `QSqlDatabase`), the existing `HttpClient`/`FakeRelayServer` test fixtures.

## Global Constraints

- No changes to kypost-server (a separate repo) — per the design doc's scope decision, `isSelf` is read-only on this client; the toggle stays web-frontend-only.
- No new "mark as my contact" button or edit-form field — display/sort only (badge in `ContactsList.qml`/`ContactDetail.qml`).
- `contactCard` consumption must fit the existing single-email/single-phone, no-birthday/no-address create/edit form contract — don't expand the form.
- Backend build: `cmake -B build -S .` (once) then `cmake --build build` must succeed with zero errors; `ctest --test-dir build` must pass (per `README.md`'s documented workflow).
- No new dependencies.
- Full field/scope reference: `docs/superpowers/specs/2026-07-18-contact-self-and-pgp-qr-card-design.md`.

---

### Task 1: `isSelf`/`mergedUIDs`/`mergedInto` on the Contact model

**Files:**
- Modify: `core/models/Contact.h`
- Test: `tests/core/models/ContactTest.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `Contact::isSelf` (`bool`, default `false`), `Contact::mergedUIDs` (`QVector<QString>`), `Contact::mergedInto` (`std::optional<QString>`). Every later task reads/writes these exact names.

- [ ] **Step 1: Write the failing test**

In `tests/core/models/ContactTest.cpp`, extend `ContactTest::defaultConstructs()`. Find:

```cpp
    QVERIFY(contact.customFields.isEmpty());
    QVERIFY(!contact.pronouns.has_value());
}
```

Replace with:

```cpp
    QVERIFY(contact.customFields.isEmpty());
    QVERIFY(!contact.pronouns.has_value());
    QCOMPARE(contact.isSelf, false);
    QVERIFY(contact.mergedUIDs.isEmpty());
    QVERIFY(!contact.mergedInto.has_value());
}
```

Then extend `ContactTest::populatesAndCompares()`. Find:

```cpp
    contact.customFields = {ContactCustomFieldEntry{QStringLiteral("Employee ID"), QStringLiteral("42")}};
    contact.pronouns = QStringLiteral("she/her");

    Contact copy = contact;
```

Replace with:

```cpp
    contact.customFields = {ContactCustomFieldEntry{QStringLiteral("Employee ID"), QStringLiteral("42")}};
    contact.pronouns = QStringLiteral("she/her");
    contact.isSelf = true;
    contact.mergedUIDs = {QStringLiteral("merged-1"), QStringLiteral("merged-2")};
    contact.mergedInto = QStringLiteral("survivor-uid");

    Contact copy = contact;
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target ContactTest && ./build/tests/ContactTest`

Expected: FAIL to compile — `contact.isSelf`/`contact.mergedUIDs`/`contact.mergedInto` don't exist yet.

- [ ] **Step 3: Add the fields**

In `core/models/Contact.h`, find:

```cpp
    QVector<ContactCustomFieldEntry> customFields;
    std::optional<QString> pronouns;
    bool deleted = false; // wire "deleted" (json:"deleted,omitempty" in the Go
                           // Contact struct) -- a push entry with {uid, rev,
                           // deleted:true} tombstones server-side, every
                           // other field omitted/zero.

    bool operator==(const Contact&) const = default;
};
```

Replace with:

```cpp
    QVector<ContactCustomFieldEntry> customFields;
    std::optional<QString> pronouns;
    bool deleted = false; // wire "deleted" (json:"deleted,omitempty" in the Go
                           // Contact struct) -- a push entry with {uid, rev,
                           // deleted:true} tombstones server-side, every
                           // other field omitted/zero.

    // Marks this as the caller's own contact card -- the (at most one,
    // enforced server-side) contact whose fields the server folds into the
    // PGP QR key-exchange response. Read-only on this client: the only
    // server endpoint that sets it requires a browser session cookie this
    // client doesn't have (see docs/superpowers/specs/
    // 2026-07-18-contact-self-and-pgp-qr-card-design.md), so this field is
    // only ever populated by a pull() and displayed, never set locally.
    bool isSelf = false;

    // Server-side-only dedupe provenance -- mergedUIDs lists the uids a
    // survivor absorbed, mergedInto points a loser's tombstone at the
    // survivor it was folded into. Never shown in any UI here; carried
    // through unchanged (never invented) purely so an edit-and-push of a
    // dedupe-survivor contact from this client stops silently wiping this
    // data server-side (Store.Upsert only special-cases CreatedAt/IsSelf on
    // update -- anything else the client's JSON omits gets zeroed).
    QVector<QString> mergedUIDs;
    std::optional<QString> mergedInto;

    bool operator==(const Contact&) const = default;
};
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target ContactTest && ./build/tests/ContactTest`

Expected: PASS, all `ContactTest` cases.

- [ ] **Step 5: Commit**

```bash
git add core/models/Contact.h tests/core/models/ContactTest.cpp
git commit -m "Add isSelf/mergedUIDs/mergedInto fields to the Contact model"
```

---

### Task 2: Wire JSON round-trip for the new fields

**Files:**
- Modify: `core/net/ContactSyncClient.cpp`
- Test: `tests/core/net/ContactSyncClientTest.cpp`

**Interfaces:**
- Consumes: `Contact::isSelf`/`mergedUIDs`/`mergedInto` from Task 1.
- Produces: `ContactWire::contactToJson`/`contactFromJson` (already-declared in `core/net/ContactSyncClient.h`, unchanged signatures) now round-trip the three new fields. Task 5 relies on `contactFromJson` handling a JSON object that only carries a subset of `Contact`'s fields (the PGP QR `contactCard` shape) — this task doesn't change that behavior, just confirms it stays true for the new fields too (absent means default).

- [ ] **Step 1: Write the failing test**

In `tests/core/net/ContactSyncClientTest.cpp`, extend `kPopulatedContactJson`. Find:

```cpp
  "customFields": [{"label":"Employee ID","value":"42"}],
  "pronouns": "she/her"
}
)";
```

Replace with:

```cpp
  "customFields": [{"label":"Employee ID","value":"42"}],
  "pronouns": "she/her",
  "isSelf": true,
  "mergedUIDs": ["merged-1", "merged-2"],
  "mergedInto": "survivor-uid"
}
)";
```

Then find `ContactSyncClientTest::pullRoundTripMapsPopulatedAndAbsentOptionalFieldsIncludingNestedEntries()`'s assertions on the populated contact (locate the block asserting `pronouns`/`customFields` on the first `changed` entry) and add, immediately after the existing `pronouns` assertion in that function:

```cpp
    QCOMPARE(changed.at(0).isSelf, true);
    QCOMPARE(changed.at(0).mergedUIDs, QVector<QString>({QStringLiteral("merged-1"), QStringLiteral("merged-2")}));
    QCOMPARE(changed.at(0).mergedInto, std::optional<QString>(QStringLiteral("survivor-uid")));
```

Also locate the assertions on the minimal (`kMinimalContactJson`, only `uid`/`rev`) contact in the same test function and add, right after its existing absent-field assertions:

```cpp
    QCOMPARE(changed.at(1).isSelf, false);
    QVERIFY(changed.at(1).mergedUIDs.isEmpty());
    QVERIFY(!changed.at(1).mergedInto.has_value());
```

Finally, extend `ContactSyncClientTest::pushRoundTripSendsExactFieldNamesIncludingEmptyUidCreate()`: find where it builds the `Contact` it pushes (look for the `contact.pronouns = ...` line in that test) and add right after it:

```cpp
    contact.isSelf = true;
    contact.mergedUIDs = {QStringLiteral("merged-1")};
    contact.mergedInto = QStringLiteral("survivor-uid");
```

Then find that same test's assertions on the received JSON body (the `QJsonObject sent = ...` / field-by-field `QCOMPARE`s) and add:

```cpp
    QCOMPARE(sent.value(QStringLiteral("isSelf")).toBool(), true);
    QCOMPARE(sent.value(QStringLiteral("mergedUIDs")).toArray().size(), 1);
    QCOMPARE(sent.value(QStringLiteral("mergedUIDs")).toArray().first().toString(), QStringLiteral("merged-1"));
    QCOMPARE(sent.value(QStringLiteral("mergedInto")).toString(), QStringLiteral("survivor-uid"));
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target ContactSyncClientTest && ./build/tests/ContactSyncClientTest`

Expected: FAIL — the new `QCOMPARE`s report `isSelf`/`mergedUIDs`/`mergedInto` as default-valued (`false`/empty/`nullopt`) on both the parsed-from-JSON contact and the sent JSON body, since neither direction is wired yet.

- [ ] **Step 3: Wire the fields into `contactToJson`/`contactFromJson`**

In `core/net/ContactSyncClient.cpp`, find:

```cpp
    putOptional(obj, QStringLiteral("pronouns"), contact.pronouns);
    if (contact.deleted)
        obj[QStringLiteral("deleted")] = true;
    return obj;
}
```

Replace with:

```cpp
    putOptional(obj, QStringLiteral("pronouns"), contact.pronouns);
    if (contact.isSelf)
        obj[QStringLiteral("isSelf")] = true;
    obj[QStringLiteral("mergedUIDs")] = stringListToJson(contact.mergedUIDs);
    putOptional(obj, QStringLiteral("mergedInto"), contact.mergedInto);
    if (contact.deleted)
        obj[QStringLiteral("deleted")] = true;
    return obj;
}
```

Then find:

```cpp
    contact.pronouns = takeOptional(obj, QStringLiteral("pronouns"));
    contact.deleted = obj.value(QStringLiteral("deleted")).toBool();
    return contact;
}
```

Replace with:

```cpp
    contact.pronouns = takeOptional(obj, QStringLiteral("pronouns"));
    contact.isSelf = obj.value(QStringLiteral("isSelf")).toBool();
    contact.mergedUIDs = stringListFromJson(obj.value(QStringLiteral("mergedUIDs")).toArray());
    contact.mergedInto = takeOptional(obj, QStringLiteral("mergedInto"));
    contact.deleted = obj.value(QStringLiteral("deleted")).toBool();
    return contact;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target ContactSyncClientTest && ./build/tests/ContactSyncClientTest`

Expected: PASS, all `ContactSyncClientTest` cases.

- [ ] **Step 5: Commit**

```bash
git add core/net/ContactSyncClient.cpp tests/core/net/ContactSyncClientTest.cpp
git commit -m "Round-trip isSelf/mergedUIDs/mergedInto through contact sync JSON"
```

---

### Task 3: SQLite schema + DAO round-trip

**Files:**
- Create: `core/db/migrations/004_contact_self_and_merged.sql`
- Modify: `core/db/ContactDao.cpp`
- Test: `tests/core/db/ContactDaoTest.cpp`, `tests/core/db/DatabaseTest.cpp`

**Interfaces:**
- Consumes: `Contact::isSelf`/`mergedUIDs`/`mergedInto` from Task 1.
- Produces: `contacts` table gains `is_self`, `merged_uids_json`, `merged_into` columns; `ContactDao::insertOrReplace`/`findById`/`findAll` round-trip them. Nothing later in this plan depends on DAO internals directly (Task 4 goes through `ContactSyncRepository::contacts()`, unaffected by this task's shape).

- [ ] **Step 1: Write the failing tests**

In `tests/core/db/ContactDaoTest.cpp`, extend `ContactDaoTest::roundTripsInsertUpdateDelete()`. Find:

```cpp
    contact.customFields = {ContactCustomFieldEntry{QStringLiteral("Employee ID"), QStringLiteral("42")}};
    contact.pronouns = QStringLiteral("she/her");

    QVERIFY(dao.insertOrReplace(contact));
```

Replace with:

```cpp
    contact.customFields = {ContactCustomFieldEntry{QStringLiteral("Employee ID"), QStringLiteral("42")}};
    contact.pronouns = QStringLiteral("she/her");
    contact.isSelf = true;
    contact.mergedUIDs = {QStringLiteral("merged-1"), QStringLiteral("merged-2")};
    contact.mergedInto = QStringLiteral("survivor-uid");

    QVERIFY(dao.insertOrReplace(contact));
```

That same test already does `QCOMPARE(*found, contact)` and `QCOMPARE(*refetched, updated)` further down, which will now exercise the new fields via `Contact::operator==` with no further test-code changes needed — full-struct comparison already covers them.

In `tests/core/db/DatabaseTest.cpp`, find:

```cpp
    // 3 migrations on disk (001_initial, 002_native_contact_links,
    // 003_extended_contact_fields) -- bumping this when a migration is
    // added is how this test proves the loop in Database::open() actually
    // walks version+1..N end-to-end.
    QCOMPARE(versionQuery.value(0).toInt(), 3);
```

Replace with:

```cpp
    // 4 migrations on disk (001_initial, 002_native_contact_links,
    // 003_extended_contact_fields, 004_contact_self_and_merged) -- bumping
    // this when a migration is added is how this test proves the loop in
    // Database::open() actually walks version+1..N end-to-end.
    QCOMPARE(versionQuery.value(0).toInt(), 4);
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target ContactDaoTest DatabaseTest && ./build/tests/DatabaseTest && ./build/tests/ContactDaoTest`

Expected: `DatabaseTest` FAILs (`versionQuery.value(0).toInt()` is still `3`, not `4`, since the migration file doesn't exist yet). `ContactDaoTest` FAILs to compile or fails `QCOMPARE(*found, contact)` (SQL error: no such column `is_self`, or the round-tripped contact silently loses the new fields since the DAO doesn't bind them).

- [ ] **Step 3: Add the migration**

Create `core/db/migrations/004_contact_self_and_merged.sql`:

```sql
-- Contact self-flag + dedupe-provenance fields: the server's isSelf
-- (per-user "this is me" flag, read-only on this client -- see
-- docs/superpowers/specs/2026-07-18-contact-self-and-pgp-qr-card-design.md)
-- and mergedUIDs/mergedInto (dedupe survivor provenance, never shown in any
-- UI here, round-tripped only to stop silently wiping it on edit-and-push).

ALTER TABLE contacts ADD COLUMN is_self INTEGER DEFAULT 0;
ALTER TABLE contacts ADD COLUMN merged_uids_json TEXT DEFAULT '[]';
ALTER TABLE contacts ADD COLUMN merged_into TEXT DEFAULT NULL;
```

(No CMakeLists changes needed — `core/CMakeLists.txt`'s `file(GLOB LLAMA_MIGRATION_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/db/migrations/*.sql)` picks up new files automatically at the next CMake configure.)

- [ ] **Step 4: Wire the columns into `ContactDao.cpp`**

In `core/db/ContactDao.cpp`, find `insertOrReplace`'s SQL statement:

```cpp
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO contacts "
        "(uid, rev, created_at, updated_at, fn, given_name, family_name, middle_name, prefix, "
        "suffix, nickname, org, title, notes, birthday, emails_json, phones_json, addresses_json, "
        "groups_json, photo_ref, pgp_key, ims_json, websites_json, relations_json, events_json, "
        "phonetic_given_name, phonetic_family_name, department, custom_fields_json, pronouns) "
        "VALUES (:uid, :rev, :created_at, :updated_at, :fn, :given_name, :family_name, "
        ":middle_name, :prefix, :suffix, :nickname, :org, :title, :notes, :birthday, "
        ":emails_json, :phones_json, :addresses_json, "
        ":groups_json, :photo_ref, :pgp_key, :ims_json, :websites_json, :relations_json, :events_json, "
        ":phonetic_given_name, :phonetic_family_name, :department, :custom_fields_json, :pronouns)"));
```

Replace with:

```cpp
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO contacts "
        "(uid, rev, created_at, updated_at, fn, given_name, family_name, middle_name, prefix, "
        "suffix, nickname, org, title, notes, birthday, emails_json, phones_json, addresses_json, "
        "groups_json, photo_ref, pgp_key, ims_json, websites_json, relations_json, events_json, "
        "phonetic_given_name, phonetic_family_name, department, custom_fields_json, pronouns, "
        "is_self, merged_uids_json, merged_into) "
        "VALUES (:uid, :rev, :created_at, :updated_at, :fn, :given_name, :family_name, "
        ":middle_name, :prefix, :suffix, :nickname, :org, :title, :notes, :birthday, "
        ":emails_json, :phones_json, :addresses_json, "
        ":groups_json, :photo_ref, :pgp_key, :ims_json, :websites_json, :relations_json, :events_json, "
        ":phonetic_given_name, :phonetic_family_name, :department, :custom_fields_json, :pronouns, "
        ":is_self, :merged_uids_json, :merged_into)"));
```

Then find:

```cpp
    query.bindValue(QStringLiteral(":custom_fields_json"), encodeEntries(contact.customFields));
    query.bindValue(QStringLiteral(":pronouns"), optionalStringToVariant(contact.pronouns));
    return query.exec();
}
```

Replace with:

```cpp
    query.bindValue(QStringLiteral(":custom_fields_json"), encodeEntries(contact.customFields));
    query.bindValue(QStringLiteral(":pronouns"), optionalStringToVariant(contact.pronouns));
    query.bindValue(QStringLiteral(":is_self"), contact.isSelf);
    query.bindValue(QStringLiteral(":merged_uids_json"), encodeStringList(contact.mergedUIDs));
    query.bindValue(QStringLiteral(":merged_into"), optionalStringToVariant(contact.mergedInto));
    return query.exec();
}
```

Finally, find `contactFromQuery`'s tail:

```cpp
    contact.customFields = decodeEntries<ContactCustomFieldEntry>(
        query.value(QStringLiteral("custom_fields_json")).toString(), customFieldEntryFromJson);
    contact.pronouns = variantToOptionalString(query.value(QStringLiteral("pronouns")));
    return contact;
}
```

Replace with:

```cpp
    contact.customFields = decodeEntries<ContactCustomFieldEntry>(
        query.value(QStringLiteral("custom_fields_json")).toString(), customFieldEntryFromJson);
    contact.pronouns = variantToOptionalString(query.value(QStringLiteral("pronouns")));
    contact.isSelf = query.value(QStringLiteral("is_self")).toBool();
    contact.mergedUIDs = decodeStringList(query.value(QStringLiteral("merged_uids_json")).toString());
    contact.mergedInto = variantToOptionalString(query.value(QStringLiteral("merged_into")));
    return contact;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build --target ContactDaoTest DatabaseTest && ./build/tests/DatabaseTest && ./build/tests/ContactDaoTest`

Expected: PASS, both test binaries.

- [ ] **Step 6: Commit**

```bash
git add core/db/migrations/004_contact_self_and_merged.sql core/db/ContactDao.cpp \
        tests/core/db/ContactDaoTest.cpp tests/core/db/DatabaseTest.cpp
git commit -m "Add is_self/merged_uids/merged_into columns and DAO round-trip"
```

---

### Task 4: Sort self-contact to top + list/detail badge

**Files:**
- Modify: `app/contacts/ContactListModel.h`, `app/contacts/ContactListModel.cpp`
- Modify: `app/contacts/ContactsController.cpp`
- Test: `tests/app/contacts/ContactListModelTest.cpp`, `tests/app/contacts/ContactsControllerTest.cpp`
- Modify (manual verification only, no test harness): `app/qml/pages/ContactsList.qml`, `app/qml/pages/ContactDetail.qml`

**Interfaces:**
- Consumes: `Contact::isSelf` from Task 1.
- Produces: `ContactListModel::IsSelfRole` (role name `"isSelf"`); `ContactsController::load()` now sorts the self-flagged contact first. Nothing later in this plan depends on this task.

- [ ] **Step 1: Write the failing tests**

In `tests/app/contacts/ContactListModelTest.cpp`, add `isSelfRoleReflectsContactField` to the `private slots:` list (after `dataOutOfRangeReturnsInvalidVariant`):

```cpp
    void isSelfRoleReflectsContactField();
```

Implement it after `ContactListModelTest::dataRoundTripsEveryRoleForAPopulatedRow()`:

```cpp
void ContactListModelTest::isSelfRoleReflectsContactField()
{
    Contact self = sampleContact();
    self.isSelf = true;
    Contact other = sampleContact();
    other.uid = QStringLiteral("srv-2");
    other.isSelf = false;

    ContactListModel model;
    model.setContacts({ self, other });

    QCOMPARE(model.data(model.index(0, 0), ContactListModel::IsSelfRole).toBool(), true);
    QCOMPARE(model.data(model.index(1, 0), ContactListModel::IsSelfRole).toBool(), false);

    // roleNames() must expose exactly these 12 role-name strings for QML now.
    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.size(), 12);
    QCOMPARE(roles.value(ContactListModel::IsSelfRole), QByteArrayLiteral("isSelf"));
}
```

Also update the existing role-count assertion in `dataRoundTripsEveryRoleForAPopulatedRow()`. Find:

```cpp
    // roleNames() must expose exactly these 11 role-name strings for QML.
    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.size(), 11);
```

Replace with:

```cpp
    // roleNames() must expose exactly these 12 role-name strings for QML.
    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.size(), 12);
```

In `tests/app/contacts/ContactsControllerTest.cpp`, search for the test that calls `ContactsController::load()` after a successful `sync()` (or, if no single test isolates `load()` directly, the simplest existing sync-success test) and add a new test right after it in the `private slots:` list and its implementation:

```cpp
    void loadSortsSelfContactFirst();
```

```cpp
void ContactsControllerTest::loadSortsSelfContactFirst()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());
    GroupDao groupDao(db.handle());

    Contact notSelf;
    notSelf.uid = QStringLiteral("c-1");
    notSelf.fn = QStringLiteral("Alice");
    QVERIFY(contactDao.insertOrReplace(notSelf));

    Contact self;
    self.uid = QStringLiteral("c-2");
    self.fn = QStringLiteral("Me");
    self.isSelf = true;
    QVERIFY(contactDao.insertOrReplace(self));

    Contact alsoNotSelf;
    alsoNotSelf.uid = QStringLiteral("c-3");
    alsoNotSelf.fn = QStringLiteral("Bob");
    QVERIFY(contactDao.insertOrReplace(alsoNotSelf));

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursor.ini")));
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient syncClient(http);
    ContactSyncRepository repository(syncClient, contactDao, pendingDao, cursorStore, pairingStore);

    QTemporaryDir photoCacheDir;
    QVERIFY(photoCacheDir.isValid());
    ContactPhotoCache photoCache(photoCacheDir.path());
    ContactPhotoClient photoClient(http);
    ContactPhotoRepository photoRepository(photoClient, photoCache, pairingStore);

    GroupsClient groupsClient(http);
    GroupsRepository groupsRepository(groupsClient, groupDao, pairingStore);

    ContactsController controller(repository, groupsRepository, photoRepository);
    controller.load();

    QObject* model = controller.contactModel();
    QCOMPARE(model->property("count").isValid() ? 0 : 0, 0); // no-op: ContactListModel exposes rowCount via QAbstractListModel, not a "count" property
    auto* listModel = qobject_cast<ContactListModel*>(model);
    QVERIFY(listModel != nullptr);
    QCOMPARE(listModel->rowCount(), 3);
    QCOMPARE(listModel->contactAt(0).uid, QStringLiteral("c-2")); // self-contact sorted first
    // The two non-self contacts keep their original relative order.
    QCOMPARE(listModel->contactAt(1).uid, QStringLiteral("c-1"));
    QCOMPARE(listModel->contactAt(2).uid, QStringLiteral("c-3"));
}
```

(This test constructs the same dependency chain the existing `ContactsControllerTest` fixtures already build — check the top of the file for the exact existing includes/constructor calls for `ContactSyncRepository`/`ContactPhotoRepository`/`GroupsRepository` and reuse those constructors verbatim if their parameter order differs from above; the point under test is only `controller.load()`'s ordering, not any network call.)

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target ContactListModelTest ContactsControllerTest && ./build/tests/ContactListModelTest && ./build/tests/ContactsControllerTest`

Expected: `ContactListModelTest` FAILs to compile (`ContactListModel::IsSelfRole` doesn't exist). `ContactsControllerTest`'s new test FAILs (`contactAt(0).uid` is `"c-1"`, not `"c-2"` — `load()` doesn't sort yet).

- [ ] **Step 3: Add `IsSelfRole` to `ContactListModel`**

In `app/contacts/ContactListModel.h`, find:

```cpp
        PhotoRefRole,
    };
```

Replace with:

```cpp
        PhotoRefRole,
        // Read-only display flag -- see core/models/Contact.h's isSelf doc
        // comment for why this client never sets it, only shows it (a "You"
        // badge) and uses it to sort the flagged contact to the front of
        // the list (see ContactsController::load()).
        IsSelfRole,
    };
```

In `app/contacts/ContactListModel.cpp`, find:

```cpp
    case PhotoRefRole:
        return contact.photoRef.value_or(QString());
    default:
        return QVariant();
    }
}
```

Replace with:

```cpp
    case PhotoRefRole:
        return contact.photoRef.value_or(QString());
    case IsSelfRole:
        return contact.isSelf;
    default:
        return QVariant();
    }
}
```

Then find:

```cpp
        { PhotoRefRole, "photoRef" },
    };
}
```

Replace with:

```cpp
        { PhotoRefRole, "photoRef" },
        { IsSelfRole, "isSelf" },
    };
}
```

- [ ] **Step 4: Sort the self-contact first in `ContactsController::load()`**

In `app/contacts/ContactsController.cpp`, find:

```cpp
void ContactsController::load()
{
    m_model->setContacts(m_repository.contacts(), m_repository.pendingUids());
}
```

Replace with:

```cpp
void ContactsController::load()
{
    // Read-only display sort: the self-contact (if any) always renders
    // first, per Contact::isSelf's doc comment -- this client never sets
    // the flag itself, only reads whatever the server already sent over
    // sync. std::stable_partition keeps every other contact's relative
    // order unchanged.
    QVector<Contact> contacts = m_repository.contacts();
    std::stable_partition(contacts.begin(), contacts.end(), [](const Contact& c) { return c.isSelf; });
    m_model->setContacts(contacts, m_repository.pendingUids());
}
```

`<algorithm>` is already included at the top of this file (for `std::find_if` in the anonymous namespace above), so no new include is needed.

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build --target ContactListModelTest ContactsControllerTest && ./build/tests/ContactListModelTest && ./build/tests/ContactsControllerTest`

Expected: PASS, both test binaries.

- [ ] **Step 6: Add the badge in QML**

In `app/qml/pages/ContactsList.qml`, find:

```qml
                        Text {
                            Layout.fillWidth: true
                            text: model.fn && model.fn.length > 0 ? model.fn : i18n("Unnamed")
                            color: Theme.inkStrong
                            font.family: Theme.fontUi
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
```

Replace with:

```qml
                        Text {
                            Layout.fillWidth: true
                            text: (model.fn && model.fn.length > 0 ? model.fn : i18n("Unnamed"))
                                + (model.isSelf ? " · " + i18n("You") : "")
                            color: Theme.inkStrong
                            font.family: Theme.fontUi
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
```

In `app/qml/pages/ContactDetail.qml`, find:

```qml
                        StatusBadge {
                            active: root.synced
                            text: root.synced ? i18n("Synced") : i18n("Local")
                        }
```

Replace with:

```qml
                        StatusBadge {
                            active: root.synced
                            text: root.synced ? i18n("Synced") : i18n("Local")
                        }
                        StatusBadge {
                            visible: root.contact.isSelf === true
                            active: true
                            text: i18n("Your contact")
                        }
```

- [ ] **Step 7: Expose `isSelf` from `ContactsController::contactAt()`**

In `app/contacts/ContactsController.cpp`, find:

```cpp
    map[QStringLiteral("pronouns")] = c.pronouns.value_or(QString());

    map[QStringLiteral("deleted")] = c.deleted;
    return map;
}
```

Replace with:

```cpp
    map[QStringLiteral("pronouns")] = c.pronouns.value_or(QString());
    map[QStringLiteral("isSelf")] = c.isSelf;

    map[QStringLiteral("deleted")] = c.deleted;
    return map;
}
```

(`ContactDetail.qml`'s read-only card binds to `root.contact.isSelf`, i.e. this `contactAt()` output, not the list model's role — Step 6's `ContactDetail.qml` badge depends on this.)

- [ ] **Step 8: Manual verification**

Run the app against a test server where a contact has been flagged `isSelf` via the web frontend (or seed a local SQLite `contacts` row with `is_self = 1` directly for a quick check without a live server), then sync:
1. `ContactsList.qml`: the flagged contact is the first row and shows "· You" after its name.
2. `ContactDetail.qml`: opening that contact's read-only card shows a "Your contact" badge next to the existing Synced/Local badge.
3. No edit-form field or button exists for this flag anywhere.

- [ ] **Step 9: Commit**

```bash
git add app/contacts/ContactListModel.h app/contacts/ContactListModel.cpp app/contacts/ContactsController.cpp \
        tests/app/contacts/ContactListModelTest.cpp tests/app/contacts/ContactsControllerTest.cpp \
        app/qml/pages/ContactsList.qml app/qml/pages/ContactDetail.qml
git commit -m "Sort the user's own contact first and show a read-only badge for it"
```

---

### Task 5: `PgpQrClient` parses the `contactCard` object

**Files:**
- Modify: `core/net/PgpQrClient.h`, `core/net/PgpQrClient.cpp`
- Test: `tests/core/net/PgpQrClientTest.cpp`

**Interfaces:**
- Consumes: `ContactWire::contactFromJson(const QJsonObject&): Contact` (existing, from `core/net/ContactSyncClient.h`, unchanged by Task 2).
- Produces: `PgpQrKeyResult::contactCard` (`std::optional<Contact>`, populated iff the response has a `contactCard` object). Task 6 reads this field by name.

- [ ] **Step 1: Write the failing tests**

In `tests/core/net/PgpQrClientTest.cpp`, add two entries to the `private slots:` list (after `fetchKeySuccessParsesNameFingerprintPublicKey`):

```cpp
    void fetchKeySuccessParsesContactCardWhenPresent();
    void fetchKeySuccessOmitsContactCardWhenAbsent();
```

Add `#include "models/Contact.h"` to the top of the file, alongside the existing includes:

```cpp
#include "net/PgpQrClient.h"

#include "models/Contact.h"
#include "net/HttpClient.h"
#include "net/RelayAuth.h"
```

Implement the two new tests after `fetchKeySuccessParsesNameFingerprintPublicKey`:

```cpp
void PgpQrClientTest::fetchKeySuccessParsesContactCardWhenPresent()
{
    const QByteArray body = R"({
        "name":"Ada","fingerprint":"ABCD1234","publicKey":"-----BEGIN PGP PUBLIC KEY BLOCK-----",
        "contactCard":{
            "fn":"Ada Lovelace","org":"Analytical Engines Ltd","notes":"Pioneer of computing",
            "emails":[{"label":"work","value":"ada@example.com"}],
            "phones":[{"label":"mobile","value":"+1-555-0100"}],
            "department":"Engineering","pronouns":"she/her"
        }
    })";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl qrUrl(QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port()));
    const PgpQrKeyResult result = client.fetchKey(qrUrl);

    QVERIFY(!result.error.has_value());
    QVERIFY(result.contactCard.has_value());
    QCOMPARE(result.contactCard->fn, std::optional<QString>(QStringLiteral("Ada Lovelace")));
    QCOMPARE(result.contactCard->org, std::optional<QString>(QStringLiteral("Analytical Engines Ltd")));
    QCOMPARE(result.contactCard->emails.size(), 1);
    QCOMPARE(result.contactCard->emails.first().value, QStringLiteral("ada@example.com"));
    QCOMPARE(result.contactCard->phones.first().value, QStringLiteral("+1-555-0100"));
    QCOMPARE(result.contactCard->department, std::optional<QString>(QStringLiteral("Engineering")));
    QCOMPARE(result.contactCard->pronouns, std::optional<QString>(QStringLiteral("she/her")));
}

void PgpQrClientTest::fetchKeySuccessOmitsContactCardWhenAbsent()
{
    const QByteArray body =
        R"({"name":"Ada","fingerprint":"ABCD1234","publicKey":"-----BEGIN PGP PUBLIC KEY BLOCK-----"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);

    const QUrl qrUrl(QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port()));
    const PgpQrKeyResult result = client.fetchKey(qrUrl);

    QVERIFY(!result.error.has_value());
    QVERIFY(!result.contactCard.has_value());
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target PgpQrClientTest && ./build/tests/PgpQrClientTest`

Expected: FAIL to compile — `PgpQrKeyResult` has no `contactCard` member yet.

- [ ] **Step 3: Add `contactCard` to `PgpQrKeyResult` and parse it**

In `core/net/PgpQrClient.h`, find:

```cpp
#pragma once

#include "net/NetworkError.h"

#include <QString>
#include <QUrl>
#include <optional>

class HttpClient;
struct RelayAuth;
```

Replace with:

```cpp
#pragma once

#include "models/Contact.h"
#include "net/NetworkError.h"

#include <QString>
#include <QUrl>
#include <optional>

class HttpClient;
struct RelayAuth;
```

Then find:

```cpp
struct PgpQrKeyResult
{
    std::optional<NetworkError> error;
    int statusCode = 0;
    QString detail; // human-readable detail on error; empty otherwise
    QString name;
    QString fingerprint;
    QString publicKey;
};
```

Replace with:

```cpp
struct PgpQrKeyResult
{
    std::optional<NetworkError> error;
    int statusCode = 0;
    QString detail; // human-readable detail on error; empty otherwise
    QString name;
    QString fingerprint;
    QString publicKey;

    // Populated iff the token owner has a contact flagged Contact::isSelf
    // (server's optional "contactCard" key) -- parsed via the existing
    // ContactWire::contactFromJson rather than a second parser, since the
    // card's fields are a strict subset of Contact's own with no
    // field-name translation. Fields the card never carries (uid, rev,
    // photoRef, isSelf, ...) simply stay default-valued and are never read.
    std::optional<Contact> contactCard;
};
```

In `core/net/PgpQrClient.cpp`, find:

```cpp
#include "net/PgpQrClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonObject>
```

Replace with:

```cpp
#include "net/PgpQrClient.h"

#include "net/ContactSyncClient.h"
#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonObject>
```

Then find:

```cpp
    const QJsonObject obj = *decoded;
    out.name = obj.value(QStringLiteral("name")).toString();
    out.fingerprint = obj.value(QStringLiteral("fingerprint")).toString();
    out.publicKey = obj.value(QStringLiteral("publicKey")).toString();
    return out;
}
```

Replace with:

```cpp
    const QJsonObject obj = *decoded;
    out.name = obj.value(QStringLiteral("name")).toString();
    out.fingerprint = obj.value(QStringLiteral("fingerprint")).toString();
    out.publicKey = obj.value(QStringLiteral("publicKey")).toString();
    if (obj.value(QStringLiteral("contactCard")).isObject())
        out.contactCard = ContactWire::contactFromJson(obj.value(QStringLiteral("contactCard")).toObject());
    return out;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target PgpQrClientTest && ./build/tests/PgpQrClientTest`

Expected: PASS, all `PgpQrClientTest` cases.

- [ ] **Step 5: Commit**

```bash
git add core/net/PgpQrClient.h core/net/PgpQrClient.cpp tests/core/net/PgpQrClientTest.cpp
git commit -m "Parse the PGP QR key response's contactCard object"
```

---

### Task 6: `PgpQrController` exposes contact-card fields; wire into "scan to create a new contact"

**Files:**
- Modify: `app/pgp/PgpQrController.h`, `app/pgp/PgpQrController.cpp`
- Test: `tests/app/pgp/PgpQrControllerTest.cpp`
- Modify (manual verification only, no test harness): `app/qml/MobileRoot.qml`, `app/qml/DesktopRoot.qml`

**Interfaces:**
- Consumes: `PgpQrKeyResult::contactCard` from Task 5.
- Produces: `PgpQrController::scannedContactCardFields(): QVariantMap`, shaped to exactly the keys `ContactsController::createContact` already accepts (`org`, `notes`, `email`, `phone`, `department`, `pronouns`, `phoneticGivenName`, `phoneticFamilyName`, `ims`, `websites`, `relations`, `events`, `customFields`) — every value defaulted to empty string/empty list when no `contactCard` was present in the last scan. Nothing later in this plan consumes it; the QML steps below are the caller.

- [ ] **Step 1: Write the failing tests**

In `tests/app/pgp/PgpQrControllerTest.cpp`, add two entries to the `private slots:` list (after `scanQrPayloadSuccessPopulatesScanResult`):

```cpp
    void scanQrPayloadSuccessPopulatesContactCardFields();
    void scanQrPayloadWithNoContactCardReturnsAllEmptyFields();
```

Implement them after `PgpQrControllerTest::scanQrPayloadSuccessPopulatesScanResult()`:

```cpp
void PgpQrControllerTest::scanQrPayloadSuccessPopulatesContactCardFields()
{
    const QByteArray body = R"({
        "name":"Ada","fingerprint":"ABCD1234","publicKey":"-----BEGIN PGP PUBLIC KEY BLOCK-----",
        "contactCard":{
            "fn":"Ada Lovelace","org":"Analytical Engines Ltd","notes":"Pioneer of computing",
            "emails":[{"label":"work","value":"ada@example.com"}],
            "phones":[{"label":"mobile","value":"+1-555-0100"}],
            "department":"Engineering","pronouns":"she/her",
            "ims":[{"service":"Matrix","label":"work","value":"@ada:example.org"}],
            "websites":[{"label":"blog","value":"https://ada.example.com"}],
            "relations":[{"label":"spouse","name":"William King"}],
            "events":[{"label":"anniversary","date":"2026-06-01"}],
            "customFields":[{"label":"Employee ID","value":"42"}]
        }
    })";
    FakeRelayServer fake(httpResponse(200, "OK", body));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);
    PgpQrRepository repository(client, pairingStore);
    PgpQrController controller(repository, client);

    const QString qrUrl = QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port());
    controller.scanQrPayload(qrUrl);

    const QVariantMap fields = controller.scannedContactCardFields();
    QCOMPARE(fields.value(QStringLiteral("org")).toString(), QStringLiteral("Analytical Engines Ltd"));
    QCOMPARE(fields.value(QStringLiteral("notes")).toString(), QStringLiteral("Pioneer of computing"));
    QCOMPARE(fields.value(QStringLiteral("email")).toString(), QStringLiteral("ada@example.com"));
    QCOMPARE(fields.value(QStringLiteral("phone")).toString(), QStringLiteral("+1-555-0100"));
    QCOMPARE(fields.value(QStringLiteral("department")).toString(), QStringLiteral("Engineering"));
    QCOMPARE(fields.value(QStringLiteral("pronouns")).toString(), QStringLiteral("she/her"));

    const QVariantList ims = fields.value(QStringLiteral("ims")).toList();
    QCOMPARE(ims.size(), 1);
    QCOMPARE(ims.first().toMap().value(QStringLiteral("value")).toString(), QStringLiteral("@ada:example.org"));

    const QVariantList websites = fields.value(QStringLiteral("websites")).toList();
    QCOMPARE(websites.size(), 1);
    QCOMPARE(websites.first().toMap().value(QStringLiteral("value")).toString(), QStringLiteral("https://ada.example.com"));

    const QVariantList relations = fields.value(QStringLiteral("relations")).toList();
    QCOMPARE(relations.size(), 1);
    QCOMPARE(relations.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("William King"));

    const QVariantList events = fields.value(QStringLiteral("events")).toList();
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.first().toMap().value(QStringLiteral("date")).toString(), QStringLiteral("2026-06-01"));

    const QVariantList customFields = fields.value(QStringLiteral("customFields")).toList();
    QCOMPARE(customFields.size(), 1);
    QCOMPARE(customFields.first().toMap().value(QStringLiteral("value")).toString(), QStringLiteral("42"));
}

void PgpQrControllerTest::scanQrPayloadWithNoContactCardReturnsAllEmptyFields()
{
    const QByteArray body =
        R"({"name":"Ada","fingerprint":"ABCD1234","publicKey":"-----BEGIN PGP PUBLIC KEY BLOCK-----"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    PgpQrClient client(http);
    PgpQrRepository repository(client, pairingStore);
    PgpQrController controller(repository, client);

    const QString qrUrl = QStringLiteral("http://127.0.0.1:%1/api/pgp/qr/key?t=tok-1").arg(fake.port());
    controller.scanQrPayload(qrUrl);

    const QVariantMap fields = controller.scannedContactCardFields();
    QCOMPARE(fields.value(QStringLiteral("org")).toString(), QString());
    QCOMPARE(fields.value(QStringLiteral("email")).toString(), QString());
    QCOMPARE(fields.value(QStringLiteral("phone")).toString(), QString());
    QVERIFY(fields.value(QStringLiteral("ims")).toList().isEmpty());
    QVERIFY(fields.value(QStringLiteral("customFields")).toList().isEmpty());
}
```

Also extend the existing `clearScanResultResetsFields()` test: find its body and add, right before the closing brace:

```cpp
    QVERIFY(controller.scannedContactCardFields().value(QStringLiteral("org")).toString().isEmpty());
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target PgpQrControllerTest && ./build/tests/PgpQrControllerTest`

Expected: FAIL to compile — `PgpQrController::scannedContactCardFields()` doesn't exist yet.

- [ ] **Step 3: Implement `scannedContactCardFields()`**

In `app/pgp/PgpQrController.h`, find:

```cpp
#pragma once

#include <QObject>
#include <QString>

class PgpQrRepository;
class PgpQrClient;
```

Replace with:

```cpp
#pragma once

#include "models/Contact.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

class PgpQrRepository;
class PgpQrClient;
```

Then find:

```cpp
    // Re-arms the scan screen for another attempt (clears scannedName/
    // scannedFingerprint/scannedPublicKey/lastError).
    void clearScanResult();

signals:
```

Replace with:

```cpp
    // Re-arms the scan screen for another attempt (clears scannedName/
    // scannedFingerprint/scannedPublicKey/lastError).
    void clearScanResult();

    // The shareable subset of the scanned person's contact details (server's
    // optional "contactCard" on the key response, see PgpQrClient::
    // fetchKey()), reshaped to exactly the field keys
    // ContactsController::createContact/updateContact already accept (org,
    // notes, email, phone, department, pronouns, phoneticGivenName,
    // phoneticFamilyName, ims, websites, relations, events, customFields) --
    // every value defaults to an empty string/list when no contactCard was
    // present in the last scan (or none has been made yet). fn/pgpKey are
    // deliberately NOT included here -- callers already have those from
    // scannedName()/scannedPublicKey() (the out-of-band-confirmed identity),
    // this is only the rest of the card. birthday/addresses are omitted
    // because the create/edit form doesn't expose those fields either.
    Q_INVOKABLE QVariantMap scannedContactCardFields() const;

signals:
```

In `app/pgp/PgpQrController.cpp`, find:

```cpp
#include "pgp/PgpQrController.h"

#include "domain/PgpQrRepository.h"
#include "net/NetworkError.h"
#include "net/PgpQrClient.h"
```

Replace with:

```cpp
#include "pgp/PgpQrController.h"

#include "domain/PgpQrRepository.h"
#include "net/NetworkError.h"
#include "net/PgpQrClient.h"

#include <QVariantList>
```

Then find the `namespace { ... isSafeQrTarget ... } // namespace` block and add small entry-to-map helpers right after it (before `PgpQrController::PgpQrController`), mirroring `ContactsController.cpp`'s own local helpers for the same entry types:

```cpp
} // namespace

namespace {

QVariantMap imEntryToMap(const ContactImEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("service")] = entry.service.value_or(QString());
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("value")] = entry.value;
    return map;
}

QVariantMap urlEntryToMap(const ContactUrlEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("value")] = entry.value;
    return map;
}

QVariantMap relationEntryToMap(const ContactRelationEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("name")] = entry.name;
    return map;
}

QVariantMap eventEntryToMap(const ContactEventEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("date")] = entry.date;
    return map;
}

QVariantMap customFieldEntryToMap(const ContactCustomFieldEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label;
    map[QStringLiteral("value")] = entry.value;
    return map;
}

template <typename T, typename ToMapFn>
QVariantList entriesToVariantList(const QVector<T>& entries, ToMapFn toMap)
{
    QVariantList list;
    list.reserve(entries.size());
    for (const T& entry : entries)
        list.append(toMap(entry));
    return list;
}

} // namespace
```

Note: the block above adds a *second* anonymous namespace to the file, after the existing `isSafeQrTarget` one — valid C++ (each `namespace { ... }` in a translation unit contributes to the same unnamed namespace), and keeps this task's diff additive rather than reordering `isSafeQrTarget`.

Now add the `m_scannedContactCard` member. In `app/pgp/PgpQrController.h`, find:

```cpp
    QString m_scannedName;
    QString m_scannedFingerprint;
    QString m_scannedPublicKey;
};
```

Replace with:

```cpp
    QString m_scannedName;
    QString m_scannedFingerprint;
    QString m_scannedPublicKey;
    Contact m_scannedContactCard;
};
```

In `app/pgp/PgpQrController.cpp`, find `scanQrPayload`'s success branch:

```cpp
    if (!result.error.has_value()) {
        setLastError(QString());
        m_scannedName = result.name;
        m_scannedFingerprint = result.fingerprint;
        m_scannedPublicKey = result.publicKey;
        emit scanResultChanged();
        return;
    }
```

Replace with:

```cpp
    if (!result.error.has_value()) {
        setLastError(QString());
        m_scannedName = result.name;
        m_scannedFingerprint = result.fingerprint;
        m_scannedPublicKey = result.publicKey;
        m_scannedContactCard = result.contactCard.value_or(Contact());
        emit scanResultChanged();
        return;
    }
```

Find `clearScanResult()`:

```cpp
void PgpQrController::clearScanResult()
{
    setLastError(QString());
    m_scannedName.clear();
    m_scannedFingerprint.clear();
    m_scannedPublicKey.clear();
    emit scanResultChanged();
}
```

Replace with:

```cpp
void PgpQrController::clearScanResult()
{
    setLastError(QString());
    m_scannedName.clear();
    m_scannedFingerprint.clear();
    m_scannedPublicKey.clear();
    m_scannedContactCard = Contact();
    emit scanResultChanged();
}
```

Finally, add the `scannedContactCardFields()` definition at the end of the file:

```cpp
QVariantMap PgpQrController::scannedContactCardFields() const
{
    QVariantMap fields;
    fields[QStringLiteral("org")] = m_scannedContactCard.org.value_or(QString());
    fields[QStringLiteral("notes")] = m_scannedContactCard.notes.value_or(QString());
    fields[QStringLiteral("email")] =
        m_scannedContactCard.emails.isEmpty() ? QString() : m_scannedContactCard.emails.first().value;
    fields[QStringLiteral("phone")] =
        m_scannedContactCard.phones.isEmpty() ? QString() : m_scannedContactCard.phones.first().value;
    fields[QStringLiteral("department")] = m_scannedContactCard.department.value_or(QString());
    fields[QStringLiteral("pronouns")] = m_scannedContactCard.pronouns.value_or(QString());
    fields[QStringLiteral("phoneticGivenName")] = m_scannedContactCard.phoneticGivenName.value_or(QString());
    fields[QStringLiteral("phoneticFamilyName")] = m_scannedContactCard.phoneticFamilyName.value_or(QString());
    fields[QStringLiteral("ims")] = entriesToVariantList(m_scannedContactCard.ims, imEntryToMap);
    fields[QStringLiteral("websites")] = entriesToVariantList(m_scannedContactCard.websites, urlEntryToMap);
    fields[QStringLiteral("relations")] = entriesToVariantList(m_scannedContactCard.relations, relationEntryToMap);
    fields[QStringLiteral("events")] = entriesToVariantList(m_scannedContactCard.events, eventEntryToMap);
    fields[QStringLiteral("customFields")] =
        entriesToVariantList(m_scannedContactCard.customFields, customFieldEntryToMap);
    return fields;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cmake --build build --target PgpQrControllerTest && ./build/tests/PgpQrControllerTest`

Expected: PASS, all `PgpQrControllerTest` cases.

- [ ] **Step 5: Wire it into the "scan to create a new contact" QML flows**

In `app/qml/MobileRoot.qml`, find:

```qml
                onKeyScanned: function (name, publicKey) {
                    if (pgpScanContactKeyPage.targetContactDetail)
                        pgpScanContactKeyPage.targetContactDetail.applyScannedKey(name, publicKey)
                    else
                        ContactsApp.createContact({ fn: name, pgpKey: publicKey })
                    root.safePop()
                }
```

Replace with:

```qml
                onKeyScanned: function (name, publicKey) {
                    if (pgpScanContactKeyPage.targetContactDetail) {
                        pgpScanContactKeyPage.targetContactDetail.applyScannedKey(name, publicKey)
                    } else {
                        // Fold in whatever contact-card details the scanned
                        // person shared (see PgpQr.scannedContactCardFields's
                        // doc comment) -- fn/pgpKey always come from this
                        // signal's own out-of-band-confirmed values, never
                        // from the card, so they're set last and win.
                        const fields = PgpQr.scannedContactCardFields()
                        fields.fn = name
                        fields.pgpKey = publicKey
                        ContactsApp.createContact(fields)
                    }
                    root.safePop()
                }
```

In `app/qml/DesktopRoot.qml`, find:

```qml
                onKeyScanned: function (name, publicKey) {
                    if (pgpScanContactKeySheet.targetContactDetail)
                        pgpScanContactKeySheet.targetContactDetail.applyScannedKey(name, publicKey)
                    else
                        ContactsApp.createContact({ fn: name, pgpKey: publicKey })
                    pgpScanContactKeySheet.close()
                }
```

Replace with:

```qml
                onKeyScanned: function (name, publicKey) {
                    if (pgpScanContactKeySheet.targetContactDetail) {
                        pgpScanContactKeySheet.targetContactDetail.applyScannedKey(name, publicKey)
                    } else {
                        // Fold in whatever contact-card details the scanned
                        // person shared (see PgpQr.scannedContactCardFields's
                        // doc comment) -- fn/pgpKey always come from this
                        // signal's own out-of-band-confirmed values, never
                        // from the card, so they're set last and win.
                        const fields = PgpQr.scannedContactCardFields()
                        fields.fn = name
                        fields.pgpKey = publicKey
                        ContactsApp.createContact(fields)
                    }
                    pgpScanContactKeySheet.close()
                }
```

- [ ] **Step 6: Manual verification**

Against a test server where a self-contact with a rich card (org, email, phone, IM, website, relation, event, custom field) is flagged via the web frontend and paired to a *different* account than the scanning device:
1. From the Contacts list (not an existing contact's detail view), tap "Scan PGP Key" and scan that account's "My PGP QR Code" screen.
2. Confirm the fingerprint prompt, tap Save.
3. Open the newly-created contact: org/email/phone/department/pronouns/IMs/websites/relations/events/custom fields all match what was shared; the name is exactly what was shown on the confirmation screen (not anything from the card); no photo (deliberately excluded server-side).
4. Repeat via `ContactDetail.qml`'s own "Scan to add key" entry point (editing an existing contact): confirm only the PGP key field changes — no other field on the contact is touched, proving `applyScannedKey`'s path is unaffected by this task.

- [ ] **Step 7: Commit**

```bash
git add app/pgp/PgpQrController.h app/pgp/PgpQrController.cpp tests/app/pgp/PgpQrControllerTest.cpp \
        app/qml/MobileRoot.qml app/qml/DesktopRoot.qml
git commit -m "Fold the PGP QR contactCard into scan-to-create-contact flow"
```

---

## Self-Review Notes

- **Spec coverage:** `isSelf` model/wire/DAO round-trip (Tasks 1-3) ✓; read-only display + sort-to-top (Task 4) ✓; `mergedUIDs`/`mergedInto` round-trip fixing the pre-existing data-loss bug (Tasks 1-3) ✓; `contactCard` parsing (Task 5) ✓; `contactCard` consumption into the create-contact flow (Task 6) ✓. The design doc's explicit non-goals (no toggle UI, no kypost-server changes, no form field expansion) are respected throughout — no task adds a mutation path for `isSelf`.
- **Type consistency:** `Contact::isSelf`/`mergedUIDs`/`mergedInto` (Task 1) are used with identical names/types in Tasks 2-4. `ContactListModel::IsSelfRole` (Task 4) is the exact name used in its own test. `PgpQrKeyResult::contactCard` (Task 5) is read by that exact name in Task 6. `PgpQrController::scannedContactCardFields()` (Task 6) returns exactly the QVariantMap key set `ContactsController::createContact`/`applyFieldsToContact` (already existing, unmodified) accept — verified against `app/contacts/ContactsController.cpp`'s current `applyFieldsToContact` field list.
- **Scope:** Entirely within this repo (kypost-Linux); no kypost-server changes, per the user's explicit decision on the session-auth gap found during investigation.
