#include "db/ContactDao.h"
#include "db/Database.h"
#include "models/Contact.h"

#include <QTest>

class ContactDaoTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void roundTripsInsertUpdateDelete();
    void findAllAndDeleteAll();

private:
    Database m_db;
};

void ContactDaoTest::init()
{
    QVERIFY(m_db.open(QStringLiteral(":memory:")));
}

void ContactDaoTest::roundTripsInsertUpdateDelete()
{
    ContactDao dao(m_db.handle());

    Contact contact;
    contact.uid = QStringLiteral("uid-1");
    contact.rev = 1;
    contact.createdAt = QStringLiteral("2026-01-01T00:00:00Z");
    contact.updatedAt = QStringLiteral("2026-01-02T00:00:00Z");
    contact.fn = QStringLiteral("Ada Lovelace");
    contact.givenName = QStringLiteral("Ada");
    contact.familyName = QStringLiteral("Lovelace");
    contact.middleName = QStringLiteral("Augusta");
    contact.prefix = QStringLiteral("Countess");
    contact.suffix = QStringLiteral("Esq.");
    contact.nickname = QStringLiteral("Ada");
    contact.org = QStringLiteral("Analytical Engines Ltd");
    contact.title = QStringLiteral("Mathematician");
    contact.notes = QStringLiteral("Notes");
    contact.birthday = QStringLiteral("1815-12-10");
    contact.emails = {ContactEmailEntry{QStringLiteral("work"), QStringLiteral("ada@example.com")}};
    contact.phones = {ContactPhoneEntry{QStringLiteral("mobile"), QStringLiteral("+1-555-0100")}};
    contact.addresses = {ContactAddressEntry{
        QStringLiteral("home"), QStringLiteral("1 Main St"), QStringLiteral("London"),
        QStringLiteral("London"), QStringLiteral("SW1A 1AA"), QStringLiteral("UK")}};
    contact.groupIds = {QStringLiteral("group-1"), QStringLiteral("group-2")};
    contact.photoRef = QStringLiteral("photo-ref-1");
    contact.pgpKey = QStringLiteral("-----BEGIN PGP PUBLIC KEY BLOCK-----");
    contact.ims = {ContactImEntry{QStringLiteral("Matrix"), QStringLiteral("work"), QStringLiteral("@ada:example.org")}};
    contact.websites = {ContactUrlEntry{QStringLiteral("blog"), QStringLiteral("https://ada.example.com")}};
    contact.relations = {ContactRelationEntry{QStringLiteral("spouse"), QStringLiteral("William King")}};
    contact.events = {ContactEventEntry{QStringLiteral("anniversary"), QStringLiteral("2026-06-01")}};
    contact.phoneticGivenName = QStringLiteral("Ay-da");
    contact.phoneticFamilyName = QStringLiteral("Love-lace");
    contact.department = QStringLiteral("Engineering");
    contact.customFields = {ContactCustomFieldEntry{QStringLiteral("Employee ID"), QStringLiteral("42")}};
    contact.pronouns = QStringLiteral("she/her");
    contact.isSelf = true;
    contact.mergedUIDs = {QStringLiteral("merged-1"), QStringLiteral("merged-2")};
    contact.mergedInto = QStringLiteral("survivor-uid");

    QVERIFY(dao.insertOrReplace(contact));

    auto found = dao.findById(contact.uid);
    QVERIFY(found.has_value());
    QCOMPARE(*found, contact);

    Contact updated = contact;
    updated.fn = QStringLiteral("Ada King");
    updated.notes = std::nullopt;
    updated.emails.append(ContactEmailEntry{std::nullopt, QStringLiteral("ada2@example.com")});
    updated.pgpKey = std::nullopt;
    updated.department = std::nullopt;
    updated.groupIds.append(QStringLiteral("group-3"));
    updated.customFields.append(ContactCustomFieldEntry{QStringLiteral("Floor"), QStringLiteral("4")});

    QVERIFY(dao.insertOrReplace(updated));

    auto refetched = dao.findById(contact.uid);
    QVERIFY(refetched.has_value());
    QCOMPARE(*refetched, updated);
    QVERIFY(!refetched->notes.has_value());
    QCOMPARE(refetched->emails.size(), 2);
    QVERIFY(!refetched->pgpKey.has_value());
    QVERIFY(!refetched->department.has_value());
    QCOMPARE(refetched->groupIds.size(), 3);
    QCOMPARE(refetched->customFields.size(), 2);

    QVERIFY(dao.deleteById(contact.uid));
    QVERIFY(!dao.findById(contact.uid).has_value());
}

void ContactDaoTest::findAllAndDeleteAll()
{
    ContactDao dao(m_db.handle());

    Contact contact1;
    contact1.uid = QStringLiteral("uid-a");
    QVERIFY(dao.insertOrReplace(contact1));

    Contact contact2;
    contact2.uid = QStringLiteral("uid-b");
    QVERIFY(dao.insertOrReplace(contact2));

    QCOMPARE(dao.findAll().size(), 2);

    QVERIFY(dao.deleteAll());
    QCOMPARE(dao.findAll().size(), 0);
}

QTEST_GUILESS_MAIN(ContactDaoTest)
#include "ContactDaoTest.moc"
