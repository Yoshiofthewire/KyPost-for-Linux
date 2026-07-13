#include "db/Database.h"
#include "db/EmailDao.h"
#include "models/Email.h"

#include <QTest>

class EmailDaoTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void roundTripsInsertUpdateDelete();
    void findsByFolderAndAll();

private:
    Database m_db;
};

void EmailDaoTest::init()
{
    QVERIFY(m_db.open(QStringLiteral(":memory:")));
}

void EmailDaoTest::roundTripsInsertUpdateDelete()
{
    EmailDao dao(m_db.handle());

    Email email;
    email.messageId = QStringLiteral("msg-1");
    email.folder = QStringLiteral("INBOX");
    email.sender = QStringLiteral("a@example.com");
    email.sentTo = QStringLiteral("b@example.com");
    email.cc = QStringLiteral("c@example.com");
    email.bcc = QStringLiteral("d@example.com");
    email.subject = QStringLiteral("Subject");
    email.preview = QStringLiteral("Preview");
    email.body = QStringLiteral("Body text");
    email.label = QStringLiteral("Label");
    email.keywords = QStringList{QStringLiteral("urgent"), QStringLiteral("work")};
    email.status = QStringLiteral("unread");
    email.atUtc = QStringLiteral("2026-01-01T00:00:00Z");
    email.hasAttachments = true;
    email.sourceMode = QStringLiteral("sync");

    QVERIFY(dao.insertOrReplace(email));

    auto found = dao.findById(email.messageId);
    QVERIFY(found.has_value());
    QCOMPARE(*found, email);

    Email updated = email;
    updated.subject = QStringLiteral("Updated subject");
    updated.body = std::nullopt;
    updated.keywords = QStringList{QStringLiteral("later")};
    updated.hasAttachments = false;
    QVERIFY(dao.insertOrReplace(updated));

    auto refetched = dao.findById(email.messageId);
    QVERIFY(refetched.has_value());
    QCOMPARE(*refetched, updated);
    QVERIFY(!refetched->body.has_value());

    QVERIFY(dao.deleteById(email.messageId));
    QVERIFY(!dao.findById(email.messageId).has_value());
}

void EmailDaoTest::findsByFolderAndAll()
{
    EmailDao dao(m_db.handle());

    Email inboxEmail;
    inboxEmail.messageId = QStringLiteral("msg-inbox");
    inboxEmail.folder = QStringLiteral("INBOX");
    inboxEmail.atUtc = QStringLiteral("2026-01-01T00:00:00Z");
    QVERIFY(dao.insertOrReplace(inboxEmail));

    Email sentEmail;
    sentEmail.messageId = QStringLiteral("msg-sent");
    sentEmail.folder = QStringLiteral("Sent");
    sentEmail.atUtc = QStringLiteral("2026-01-02T00:00:00Z");
    QVERIFY(dao.insertOrReplace(sentEmail));

    QCOMPARE(dao.findByFolder(QStringLiteral("INBOX")).size(), 1);
    QCOMPARE(dao.findAll().size(), 2);

    QVERIFY(dao.deleteAll());
    QCOMPARE(dao.findAll().size(), 0);
}

QTEST_GUILESS_MAIN(EmailDaoTest)
#include "EmailDaoTest.moc"
