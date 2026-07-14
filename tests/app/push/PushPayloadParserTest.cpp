#include "push/PushPayloadParser.h"

#include <QTest>

class PushPayloadParserTest : public QObject
{
    Q_OBJECT

private slots:
    void fullValidPayloadRoundTripsEveryField();
    void missingDataObjectFailsGracefully();
    void missingMessageIdFailsGracefully();
    void emptyKeywordsStringProducesEmptyStringList();
    void malformedKeywordsWithExtraCommasDropsEmpties();
    void malformedJsonFailsGracefully();
};

void PushPayloadParserTest::fullValidPayloadRoundTripsEveryField()
{
    const QByteArray body = R"({"title":"outer-title","body":"outer-body",)"
                             R"("data":{"messageId":"msg-1","sender":"a@example.com","subject":"Hello",)"
                             R"("senderName":"Alice","emailSubject":"Hello there","Keywords":"work,urgent",)"
                             R"("title":"Alice","body":"Hello there","url":"/read"}})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(result.has_value());
    QCOMPARE(result->messageId, QStringLiteral("msg-1"));
    QCOMPARE(result->sender, QStringLiteral("a@example.com"));
    QCOMPARE(result->subject, QStringLiteral("Hello"));
    QCOMPARE(result->senderName, QStringLiteral("Alice"));
    QCOMPARE(result->emailSubject, QStringLiteral("Hello there"));
    QCOMPARE(result->keywords, QStringList({ QStringLiteral("work"), QStringLiteral("urgent") }));
    // title/body must come from data's copies, not the outer envelope's.
    QCOMPARE(result->title, QStringLiteral("Alice"));
    QCOMPARE(result->body, QStringLiteral("Hello there"));
    QCOMPARE(result->url, QStringLiteral("/read"));
}

void PushPayloadParserTest::missingDataObjectFailsGracefully()
{
    const QByteArray body = R"({"title":"outer-title","body":"outer-body"})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(!result.has_value());
}

void PushPayloadParserTest::missingMessageIdFailsGracefully()
{
    const QByteArray body = R"({"title":"outer-title","body":"outer-body",)"
                             R"("data":{"sender":"a@example.com","subject":"Hello",)"
                             R"("senderName":"Alice","emailSubject":"Hello there","Keywords":"work,urgent",)"
                             R"("title":"Alice","body":"Hello there","url":"/read"}})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(!result.has_value());
}

void PushPayloadParserTest::emptyKeywordsStringProducesEmptyStringList()
{
    const QByteArray body = R"({"data":{"messageId":"msg-1","Keywords":""}})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(result.has_value());
    QVERIFY(result->keywords.isEmpty());
}

void PushPayloadParserTest::malformedKeywordsWithExtraCommasDropsEmpties()
{
    const QByteArray body = R"({"data":{"messageId":"msg-1","Keywords":"work,,urgent,"}})";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(result.has_value());
    QCOMPARE(result->keywords, QStringList({ QStringLiteral("work"), QStringLiteral("urgent") }));
}

void PushPayloadParserTest::malformedJsonFailsGracefully()
{
    const QByteArray body = "{not valid json";

    const std::optional<PushNotification> result = PushPayloadParser::parse(body);

    QVERIFY(!result.has_value());
}

QTEST_GUILESS_MAIN(PushPayloadParserTest)
#include "PushPayloadParserTest.moc"
