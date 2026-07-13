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

QTEST_GUILESS_MAIN(ContactsControllerTest)
#include "ContactsControllerTest.moc"
