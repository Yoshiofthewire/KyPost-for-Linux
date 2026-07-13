#include "net/MfaResponseClient.h"

#include "net/HttpClient.h"

#include "FakeRelayServer.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTest>

class MfaResponseClientTest : public QObject
{
    Q_OBJECT

private slots:
    void successParsesStatusAndBuildsEndpointFromServerBaseUrl();
    void conflictFrom409IsRejected();
    void unauthorizedFrom401IsRejected();
    void sentBodyHasAllFourRequiredFieldsAndApproveNotApproved();
};

// Regression coverage for the stale-Swift-shape bug this task exists to
// avoid reintroducing: the Swift reference client sends
// {challengeId, approved} with sub/hash as query params, which is NOT what
// internal/api/push_mfa_handlers.go's handlePushRespond expects. The
// four-field body below plus the "approve" (not "approved") boolean key is
// the Go-verified shape.

void MfaResponseClientTest::successParsesStatusAndBuildsEndpointFromServerBaseUrl()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"status":"approved"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const MfaResponseResult result = client.respond(serverBaseUrl, QStringLiteral("chal-1"), QStringLiteral("sub-1"),
                                                      QStringLiteral("hash-1"), QStringLiteral("device-1"), true);

    QCOMPARE(result.outcome, MfaResponseOutcome::Success);
    QVERIFY(result.status.has_value());
    QCOMPARE(*result.status, QStringLiteral("approved"));

    QVERIFY(fake.receivedRequest().contains("POST /api/mfa/push/respond HTTP/1.1"));
    // No query-param auth on this endpoint.
    QVERIFY(!fake.receivedRequest().contains("?"));
}

void MfaResponseClientTest::conflictFrom409IsRejected()
{
    FakeRelayServer fake(httpResponse(409, "Conflict", R"({"error":"already resolved","status":"approved"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const MfaResponseResult result = client.respond(serverBaseUrl, QStringLiteral("chal-1"), QStringLiteral("sub-1"),
                                                      QStringLiteral("hash-1"), QStringLiteral("device-1"), true);

    QCOMPARE(result.outcome, MfaResponseOutcome::Rejected);
    // status is optional on the 409 path per the brief, but surfaced here
    // since the server included it.
    QVERIFY(result.status.has_value());
    QCOMPARE(*result.status, QStringLiteral("approved"));
}

void MfaResponseClientTest::unauthorizedFrom401IsRejected()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "Unauthorized\n"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const MfaResponseResult result = client.respond(serverBaseUrl, QStringLiteral("chal-1"), QStringLiteral("sub-1"),
                                                      QStringLiteral("hash-1"), QStringLiteral("device-1"), false);

    QCOMPARE(result.outcome, MfaResponseOutcome::Rejected);
}

void MfaResponseClientTest::sentBodyHasAllFourRequiredFieldsAndApproveNotApproved()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"status":"rejected"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    client.respond(serverBaseUrl, QStringLiteral("chal-42"), QStringLiteral("sub-42"), QStringLiteral("hash-42"),
                    QStringLiteral("device-42"), false);

    const QJsonObject sent = fake.receivedJsonBody();

    // The four required string fields, all present.
    QCOMPARE(sent.value(QStringLiteral("challengeId")).toString(), QStringLiteral("chal-42"));
    QCOMPARE(sent.value(QStringLiteral("subscriberId")).toString(), QStringLiteral("sub-42"));
    QCOMPARE(sent.value(QStringLiteral("subscriberHash")).toString(), QStringLiteral("hash-42"));
    QCOMPARE(sent.value(QStringLiteral("deviceId")).toString(), QStringLiteral("device-42"));

    // The regression this task exists to prevent: the boolean key on the
    // wire must be "approve", not the stale Swift client's "approved".
    QVERIFY(sent.contains(QStringLiteral("approve")));
    QVERIFY(!sent.contains(QStringLiteral("approved")));
    QCOMPARE(sent.value(QStringLiteral("approve")).toBool(), false);

    // Exactly these four fields plus approve — no leftover sub/hash query
    // params and no stray fields from the stale shape.
    QCOMPARE(sent.size(), 5);
}

QTEST_GUILESS_MAIN(MfaResponseClientTest)
#include "MfaResponseClientTest.moc"
