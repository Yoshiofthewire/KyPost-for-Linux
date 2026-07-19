#include "net/DeregisterClient.h"

#include "net/HttpClient.h"

#include "FakeRelayServer.h"

#include <QNetworkAccessManager>
#include <QTest>

class DeregisterClientTest : public QObject
{
    Q_OBJECT

private slots:
    void success200SendsDeviceHeadersAndEmptyBody();
    void unauthorizedFrom401();
    void serverErrorMapsToFailure();
};

void DeregisterClientTest::success200SendsDeviceHeadersAndEmptyBody()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    DeregisterClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const DeregisterResult result = client.deregister(serverBaseUrl, QStringLiteral("device-1"), QStringLiteral("secret-1"));

    QCOMPARE(result.outcome, DeregisterOutcome::Success);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("POST /api/notifications/native/deregister HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Device-Id: device-1"));
    QVERIFY(request.contains("X-Kypost-Device-Secret: secret-1"));
    QVERIFY(!request.contains("?"));
}

void DeregisterClientTest::unauthorizedFrom401()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "invalid device credentials\n"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    DeregisterClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const DeregisterResult result = client.deregister(serverBaseUrl, QStringLiteral("device-1"), QStringLiteral("secret-1"));

    QCOMPARE(result.outcome, DeregisterOutcome::Unauthorized);
}

void DeregisterClientTest::serverErrorMapsToFailure()
{
    FakeRelayServer fake(httpResponse(500, "Internal Server Error", "boom"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    DeregisterClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const DeregisterResult result = client.deregister(serverBaseUrl, QStringLiteral("device-1"), QStringLiteral("secret-1"));

    QCOMPARE(result.outcome, DeregisterOutcome::Failure);
}

QTEST_GUILESS_MAIN(DeregisterClientTest)
#include "DeregisterClientTest.moc"
