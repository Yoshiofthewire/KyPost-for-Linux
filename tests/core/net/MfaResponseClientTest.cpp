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
    void sentRequestCarriesDeviceHeadersAndSlimBody();
};

// Regression coverage for the Go-verified shape: internal/api/
// push_mfa_handlers.go's handlePushRespond authenticates via
// X-Kypost-Device-Id/X-Kypost-Device-Secret headers, with only
// {challengeId, approve} in the body -- no credentials ride in the JSON.

void MfaResponseClientTest::successParsesStatusAndBuildsEndpointFromServerBaseUrl()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"status":"approved"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const MfaResponseResult result =
        client.respond(serverBaseUrl, QStringLiteral("chal-1"), QStringLiteral("device-1"), QStringLiteral("secret-1"), true);

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
    const MfaResponseResult result =
        client.respond(serverBaseUrl, QStringLiteral("chal-1"), QStringLiteral("device-1"), QStringLiteral("secret-1"), true);

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
    const MfaResponseResult result = client.respond(serverBaseUrl, QStringLiteral("chal-1"), QStringLiteral("device-1"),
                                                      QStringLiteral("secret-1"), false);

    QCOMPARE(result.outcome, MfaResponseOutcome::Rejected);
}

void MfaResponseClientTest::sentRequestCarriesDeviceHeadersAndSlimBody()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"status":"rejected"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    client.respond(serverBaseUrl, QStringLiteral("chal-42"), QStringLiteral("device-42"), QStringLiteral("secret-42"), false);

    QVERIFY(fake.receivedRequest().contains("X-Kypost-Device-Id: device-42"));
    QVERIFY(fake.receivedRequest().contains("X-Kypost-Device-Secret: secret-42"));

    const QJsonObject sent = fake.receivedJsonBody();

    QCOMPARE(sent.value(QStringLiteral("challengeId")).toString(), QStringLiteral("chal-42"));
    QVERIFY(!sent.contains(QStringLiteral("subscriberId")));
    QVERIFY(!sent.contains(QStringLiteral("subscriberHash")));
    QVERIFY(!sent.contains(QStringLiteral("deviceId")));

    // The regression this task exists to prevent: the boolean key on the
    // wire must be "approve", not the stale Swift client's "approved".
    QVERIFY(sent.contains(QStringLiteral("approve")));
    QVERIFY(!sent.contains(QStringLiteral("approved")));
    QCOMPARE(sent.value(QStringLiteral("approve")).toBool(), false);

    // Exactly these two fields — no leftover credential fields.
    QCOMPARE(sent.size(), 2);
}

QTEST_GUILESS_MAIN(MfaResponseClientTest)
#include "MfaResponseClientTest.moc"
