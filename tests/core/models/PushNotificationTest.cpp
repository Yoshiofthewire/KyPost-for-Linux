#include "models/PushNotification.h"

#include <QTest>

class PushNotificationTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructs();
    void populatesAndCompares();
};

void PushNotificationTest::defaultConstructs()
{
    PushNotification push;
    QVERIFY(push.messageId.isEmpty());
    QVERIFY(push.sender.isEmpty());
    QVERIFY(push.subject.isEmpty());
    QVERIFY(push.senderName.isEmpty());
    QVERIFY(push.emailSubject.isEmpty());
    QVERIFY(push.keywords.isEmpty());
    QVERIFY(push.title.isEmpty());
    QVERIFY(push.body.isEmpty());
    QVERIFY(push.url.isEmpty());
}

void PushNotificationTest::populatesAndCompares()
{
    PushNotification push;
    push.messageId = QStringLiteral("msg-1");
    push.sender = QStringLiteral("a@example.com");
    push.subject = QStringLiteral("Hello");
    push.senderName = QStringLiteral("Alice");
    push.emailSubject = QStringLiteral("Hello there");
    push.keywords = QStringList{QStringLiteral("work"), QStringLiteral("urgent")};
    push.title = QStringLiteral("Alice");
    push.body = QStringLiteral("Hello there");
    push.url = QStringLiteral("/read");

    PushNotification copy = push;
    QCOMPARE(copy, push);

    PushNotification assigned;
    assigned = push;
    QCOMPARE(assigned, push);

    PushNotification different = push;
    different.keywords = QStringList{QStringLiteral("other")};
    QVERIFY(different != push);
}

QTEST_APPLESS_MAIN(PushNotificationTest)
#include "PushNotificationTest.moc"
