#include "models/Email.h"

#include <QTest>

class EmailTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructs();
    void populatesAndCompares();
};

void EmailTest::defaultConstructs()
{
    Email email;
    QVERIFY(email.messageId.isEmpty());
    QVERIFY(email.folder.isEmpty());
    QVERIFY(email.sender.isEmpty());
    QVERIFY(email.sentTo.isEmpty());
    QVERIFY(email.cc.isEmpty());
    QVERIFY(email.bcc.isEmpty());
    QVERIFY(email.subject.isEmpty());
    QVERIFY(email.preview.isEmpty());
    QVERIFY(!email.body.has_value());
    QVERIFY(email.label.isEmpty());
    QVERIFY(email.keywords.isEmpty());
    QVERIFY(email.status.isEmpty());
    QVERIFY(email.atUtc.isEmpty());
    QVERIFY(!email.hasAttachments);
    QVERIFY(email.sourceMode.isEmpty());
}

void EmailTest::populatesAndCompares()
{
    Email email;
    email.messageId = QStringLiteral("msg-1");
    email.folder = QStringLiteral("INBOX");
    email.sender = QStringLiteral("a@example.com");
    email.sentTo = QStringLiteral("b@example.com");
    email.cc = QStringLiteral("c@example.com");
    email.bcc = QStringLiteral("d@example.com");
    email.subject = QStringLiteral("Hello");
    email.preview = QStringLiteral("Hi there");
    email.body = QStringLiteral("Full body text");
    email.label = QStringLiteral("Important");
    email.keywords = QStringList{QStringLiteral("work"), QStringLiteral("urgent")};
    email.status = QStringLiteral("unread");
    email.atUtc = QStringLiteral("2026-07-12T10:00:00Z");
    email.hasAttachments = true;
    email.sourceMode = QStringLiteral("relay");

    Email copy = email;
    QCOMPARE(copy, email);

    Email assigned;
    assigned = email;
    QCOMPARE(assigned, email);

    Email different = email;
    different.subject = QStringLiteral("Something else");
    QVERIFY(!(different == email));
    QVERIFY(different != email);

    Email noBody = email;
    noBody.body.reset();
    QVERIFY(noBody != email);
}

QTEST_APPLESS_MAIN(EmailTest)
#include "EmailTest.moc"
