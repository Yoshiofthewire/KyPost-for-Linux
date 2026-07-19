#include "net/GroupsClient.h"

#include "models/Group.h"
#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include "FakeRelayServer.h"

#include <QNetworkAccessManager>
#include <QTest>

class GroupsClientTest : public QObject
{
    Q_OBJECT

private slots:
    void fetchParsesJsonArrayIntoGroups();
    void fetchSendsAuthAsHeadersAndGetsCorrectPath();
    void fetchOnEmptyArrayReturnsEmptyGroupsNoError();
    void fetchUnauthorizedFrom401DegradesGracefullyToEmptyResult();
    void fetchOnMalformedBodyReturnsDecodingErrorNotCrash();
};

void GroupsClientTest::fetchParsesJsonArrayIntoGroups()
{
    const QByteArray body = R"([{"id":"group-1","name":"Family","rev":1},)"
                             R"({"id":"group-2","name":"Work","rev":3}])";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    GroupsClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("device-1"), QStringLiteral("secret-1") };
    const GroupsFetchResult result = client.fetch(serverBaseUrl, auth);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.groups.size(), 2);
    QCOMPARE(result.groups.at(0).id, QStringLiteral("group-1"));
    QCOMPARE(result.groups.at(0).name, QStringLiteral("Family"));
    QCOMPARE(result.groups.at(0).rev, qint64(1));
    QCOMPARE(result.groups.at(1).id, QStringLiteral("group-2"));
    QCOMPARE(result.groups.at(1).name, QStringLiteral("Work"));
    QCOMPARE(result.groups.at(1).rev, qint64(3));
}

void GroupsClientTest::fetchSendsAuthAsHeadersAndGetsCorrectPath()
{
    FakeRelayServer fake(httpResponse(200, "OK", "[]"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    GroupsClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("device-9"), QStringLiteral("secret-9") };
    client.fetch(serverBaseUrl, auth);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/groups HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Device-Id: device-9"));
    QVERIFY(request.contains("X-Kypost-Device-Secret: secret-9"));
    QVERIFY(!request.contains("device=device-9"));
    QVERIFY(!request.contains("secret=secret-9"));
}

void GroupsClientTest::fetchOnEmptyArrayReturnsEmptyGroupsNoError()
{
    FakeRelayServer fake(httpResponse(200, "OK", "[]"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    GroupsClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("device-1"), QStringLiteral("secret-1") };
    const GroupsFetchResult result = client.fetch(serverBaseUrl, auth);

    QVERIFY(!result.error.has_value());
    QVERIFY(result.groups.isEmpty());
}

void GroupsClientTest::fetchUnauthorizedFrom401DegradesGracefullyToEmptyResult()
{
    // Global Constraint (task-2-brief.md): GroupsClient must degrade
    // gracefully on 401/error -- empty groups, error set, never a crash --
    // since the backend endpoint this depends on is an unverified external
    // dependency that may not be deployed.
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "Unauthorized\n"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    GroupsClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("device-1"), QStringLiteral("secret-1") };
    const GroupsFetchResult result = client.fetch(serverBaseUrl, auth);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Unauthorized);
    QVERIFY(result.groups.isEmpty());
}

void GroupsClientTest::fetchOnMalformedBodyReturnsDecodingErrorNotCrash()
{
    // A 200 response whose body isn't a JSON array at all (e.g. an object,
    // or garbage) must decode-fail gracefully, not crash -- same "never
    // assume the backend is deployed correctly" caveat as the 401 case.
    FakeRelayServer fake(httpResponse(200, "OK", R"({"not":"an array"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    GroupsClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("device-1"), QStringLiteral("secret-1") };
    const GroupsFetchResult result = client.fetch(serverBaseUrl, auth);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Decoding);
    QVERIFY(result.groups.isEmpty());
}

QTEST_GUILESS_MAIN(GroupsClientTest)
#include "GroupsClientTest.moc"
