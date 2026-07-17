#include "contacts/ContactsController.h"

#include "contacts/ContactListModel.h"
#include "db/ContactDao.h"
#include "db/Database.h"
#include "db/PendingContactChangeDao.h"
#include "domain/ContactSyncRepository.h"
#include "domain/PairingStore.h"
#include "net/ContactSyncClient.h"
#include "net/HttpClient.h"
#include "stores/CursorStore.h"
#include "stores/SecureStoreFile.h"

#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>

class ContactsControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void updateContactPreservesEmailEntriesBeyondIndexZero();
    void createContactRejectsBlankName();
    void updateContactRejectsBlankName();
    void syncWithoutPairingSetsNotPairedMessage();
    void createAndUpdateContactRoundTripExtendedFields();
};

void ContactsControllerTest::updateContactPreservesEmailEntriesBeyondIndexZero()
{
    // Regression test for the "preserve extras beyond index 0" rule --
    // this is the one rule most likely to regress silently, per the task
    // brief.
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired, fine: this test never syncs

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);

    Contact seed;
    seed.uid = QStringLiteral("srv-1");
    seed.rev = 1; // already synced
    seed.fn = QStringLiteral("Old Name");
    seed.emails = { ContactEmailEntry{ std::nullopt, QStringLiteral("old@example.com") },
                     ContactEmailEntry{ QStringLiteral("work"), QStringLiteral("extra@example.com") } };
    QVERIFY(contactDao.insertOrReplace(seed));

    ContactsController controller(repository);

    QVariantMap fields;
    fields[QStringLiteral("fn")] = QStringLiteral("New Name");
    fields[QStringLiteral("org")] = QString();
    fields[QStringLiteral("notes")] = QString();
    fields[QStringLiteral("email")] = QStringLiteral("new@example.com");
    fields[QStringLiteral("phone")] = QString();

    QVERIFY(controller.updateContact(QStringLiteral("srv-1"), fields));

    const QVariantMap updated = controller.contactAt(QStringLiteral("srv-1"));
    QCOMPARE(updated.value(QStringLiteral("fn")).toString(), QStringLiteral("New Name"));

    const QVariantList emails = updated.value(QStringLiteral("emails")).toList();
    QCOMPARE(emails.size(), 2);
    QCOMPARE(emails.at(0).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("new@example.com"));
    // index 1 survives unchanged, including its label.
    QCOMPARE(emails.at(1).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("extra@example.com"));
    QCOMPARE(emails.at(1).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("work"));

    // The model was reloaded, too.
    auto* model = qobject_cast<ContactListModel*>(controller.contactModel());
    QVERIFY(model != nullptr);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->data(model->index(0, 0), ContactListModel::FnRole).toString(), QStringLiteral("New Name"));
}

void ContactsControllerTest::createContactRejectsBlankName()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);
    ContactsController controller(repository);

    QSignalSpy errorSpy(&controller, &ContactsController::lastErrorChanged);

    QVariantMap fields;
    fields[QStringLiteral("fn")] = QStringLiteral("   "); // whitespace-only

    const QString newUid = controller.createContact(fields);

    QVERIFY(newUid.isEmpty());
    QCOMPARE(controller.lastError(), QStringLiteral("Name is required"));
    QVERIFY(errorSpy.count() >= 1);
    QVERIFY(repository.contacts().isEmpty());
}

void ContactsControllerTest::updateContactRejectsBlankName()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);

    Contact seed;
    seed.uid = QStringLiteral("srv-1");
    seed.rev = 1;
    seed.fn = QStringLiteral("Old Name");
    QVERIFY(contactDao.insertOrReplace(seed));

    ContactsController controller(repository);

    QVariantMap fields;
    fields[QStringLiteral("fn")] = QString();

    QCOMPARE(controller.updateContact(QStringLiteral("srv-1"), fields), false);
    QCOMPARE(controller.lastError(), QStringLiteral("Name is required"));

    // The seed row is untouched.
    const QVariantMap unchanged = controller.contactAt(QStringLiteral("srv-1"));
    QCOMPARE(unchanged.value(QStringLiteral("fn")).toString(), QStringLiteral("Old Name"));
}

void ContactsControllerTest::syncWithoutPairingSetsNotPairedMessage()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);
    ContactsController controller(repository);

    QSignalSpy errorSpy(&controller, &ContactsController::lastErrorChanged);
    QSignalSpy statusSpy(&controller, &ContactsController::statusMessageChanged);
    QSignalSpy busySpy(&controller, &ContactsController::isBusyChanged);

    controller.sync();

    QCOMPARE(controller.lastError(), QStringLiteral("Not paired"));
    QCOMPARE(controller.statusMessage(), QString());
    QVERIFY(errorSpy.count() >= 1);
    QCOMPARE(statusSpy.count(), 0); // was already "", stayed ""
    // isBusy toggled true then back to false around the (short-circuited,
    // no-network) sync() call.
    QVERIFY(busySpy.count() >= 2);
    QCOMPARE(controller.isBusy(), false);
}

void ContactsControllerTest::createAndUpdateContactRoundTripExtendedFields()
{
    // Task 1 of the extended-contact-fields feature: no edit-form UI
    // consumes these 12 new QVariantMap keys yet, but createContact/
    // updateContact/contactAt must round-trip them correctly since a later
    // task's edit form will rely on this contract.
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    ContactDao contactDao(db.handle());
    PendingContactChangeDao pendingDao(db.handle());

    QTemporaryDir cursorDir;
    QVERIFY(cursorDir.isValid());
    CursorStore cursorStore(cursorDir.filePath(QStringLiteral("cursors.ini")));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactSyncClient client(http);

    ContactSyncRepository repository(client, contactDao, pendingDao, cursorStore, pairingStore);
    ContactsController controller(repository);

    QVariantMap imEntry;
    imEntry[QStringLiteral("service")] = QStringLiteral("Matrix");
    imEntry[QStringLiteral("label")] = QStringLiteral("work");
    imEntry[QStringLiteral("value")] = QStringLiteral("@ada:example.org");

    QVariantMap websiteEntry;
    websiteEntry[QStringLiteral("label")] = QStringLiteral("blog");
    websiteEntry[QStringLiteral("value")] = QStringLiteral("https://ada.example.com");

    QVariantMap relationEntry;
    relationEntry[QStringLiteral("label")] = QStringLiteral("spouse");
    relationEntry[QStringLiteral("name")] = QStringLiteral("William King");

    QVariantMap eventEntry;
    eventEntry[QStringLiteral("label")] = QStringLiteral("anniversary");
    eventEntry[QStringLiteral("date")] = QStringLiteral("2026-06-01");

    QVariantMap customFieldEntry;
    customFieldEntry[QStringLiteral("label")] = QStringLiteral("Employee ID");
    customFieldEntry[QStringLiteral("value")] = QStringLiteral("42");

    QVariantMap fields;
    fields[QStringLiteral("fn")] = QStringLiteral("Ada Lovelace");
    fields[QStringLiteral("groupIds")] = QVariantList{ QStringLiteral("group-1"), QStringLiteral("group-2") };
    fields[QStringLiteral("photoRef")] = QStringLiteral("photo-ref-1");
    fields[QStringLiteral("pgpKey")] = QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----");
    fields[QStringLiteral("ims")] = QVariantList{ imEntry };
    fields[QStringLiteral("websites")] = QVariantList{ websiteEntry };
    fields[QStringLiteral("relations")] = QVariantList{ relationEntry };
    fields[QStringLiteral("events")] = QVariantList{ eventEntry };
    fields[QStringLiteral("phoneticGivenName")] = QStringLiteral("Ay-da");
    fields[QStringLiteral("phoneticFamilyName")] = QStringLiteral("Love-lace");
    fields[QStringLiteral("department")] = QStringLiteral("Engineering");
    fields[QStringLiteral("customFields")] = QVariantList{ customFieldEntry };
    fields[QStringLiteral("pronouns")] = QStringLiteral("she/her");

    const QString newUid = controller.createContact(fields);
    QVERIFY(!newUid.isEmpty());

    const QVariantMap created = controller.contactAt(newUid);
    QCOMPARE(created.value(QStringLiteral("groupIds")).toStringList(),
              QStringList({ QStringLiteral("group-1"), QStringLiteral("group-2") }));
    QCOMPARE(created.value(QStringLiteral("photoRef")).toString(), QStringLiteral("photo-ref-1"));
    QCOMPARE(created.value(QStringLiteral("pgpKey")).toString(), QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----"));
    QCOMPARE(created.value(QStringLiteral("phoneticGivenName")).toString(), QStringLiteral("Ay-da"));
    QCOMPARE(created.value(QStringLiteral("phoneticFamilyName")).toString(), QStringLiteral("Love-lace"));
    QCOMPARE(created.value(QStringLiteral("department")).toString(), QStringLiteral("Engineering"));
    QCOMPARE(created.value(QStringLiteral("pronouns")).toString(), QStringLiteral("she/her"));

    const QVariantList createdIms = created.value(QStringLiteral("ims")).toList();
    QCOMPARE(createdIms.size(), 1);
    QCOMPARE(createdIms.at(0).toMap().value(QStringLiteral("service")).toString(), QStringLiteral("Matrix"));
    QCOMPARE(createdIms.at(0).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("@ada:example.org"));

    const QVariantList createdWebsites = created.value(QStringLiteral("websites")).toList();
    QCOMPARE(createdWebsites.size(), 1);
    QCOMPARE(createdWebsites.at(0).toMap().value(QStringLiteral("value")).toString(),
              QStringLiteral("https://ada.example.com"));

    const QVariantList createdRelations = created.value(QStringLiteral("relations")).toList();
    QCOMPARE(createdRelations.size(), 1);
    QCOMPARE(createdRelations.at(0).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("William King"));

    const QVariantList createdEvents = created.value(QStringLiteral("events")).toList();
    QCOMPARE(createdEvents.size(), 1);
    QCOMPARE(createdEvents.at(0).toMap().value(QStringLiteral("date")).toString(), QStringLiteral("2026-06-01"));

    const QVariantList createdCustomFields = created.value(QStringLiteral("customFields")).toList();
    QCOMPARE(createdCustomFields.size(), 1);
    QCOMPARE(createdCustomFields.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Employee ID"));
    QCOMPARE(createdCustomFields.at(0).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("42"));

    // updateContact is a whole-value/whole-list replace for every one of
    // these fields, same as it already is for org/notes -- omitting a key
    // from `fields` clears it, it does not preserve the prior value.
    QVariantMap updateFields;
    updateFields[QStringLiteral("fn")] = QStringLiteral("Ada Lovelace");
    updateFields[QStringLiteral("groupIds")] = QVariantList{ QStringLiteral("group-3") };
    updateFields[QStringLiteral("pronouns")] = QStringLiteral("they/them");
    // photoRef/pgpKey/ims/websites/relations/events/phoneticGivenName/
    // phoneticFamilyName/department/customFields deliberately omitted.

    QVERIFY(controller.updateContact(newUid, updateFields));

    const QVariantMap updated = controller.contactAt(newUid);
    QCOMPARE(updated.value(QStringLiteral("groupIds")).toStringList(), QStringList({ QStringLiteral("group-3") }));
    QCOMPARE(updated.value(QStringLiteral("pronouns")).toString(), QStringLiteral("they/them"));
    QCOMPARE(updated.value(QStringLiteral("photoRef")).toString(), QString());
    QCOMPARE(updated.value(QStringLiteral("pgpKey")).toString(), QString());
    QVERIFY(updated.value(QStringLiteral("ims")).toList().isEmpty());
    QVERIFY(updated.value(QStringLiteral("websites")).toList().isEmpty());
    QVERIFY(updated.value(QStringLiteral("relations")).toList().isEmpty());
    QVERIFY(updated.value(QStringLiteral("events")).toList().isEmpty());
    QVERIFY(updated.value(QStringLiteral("customFields")).toList().isEmpty());
    QCOMPARE(updated.value(QStringLiteral("phoneticGivenName")).toString(), QString());
    QCOMPARE(updated.value(QStringLiteral("phoneticFamilyName")).toString(), QString());
    QCOMPARE(updated.value(QStringLiteral("department")).toString(), QString());
}

QTEST_GUILESS_MAIN(ContactsControllerTest)
#include "ContactsControllerTest.moc"
