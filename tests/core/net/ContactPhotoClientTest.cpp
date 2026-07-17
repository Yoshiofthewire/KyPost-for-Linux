#include "net/ContactPhotoClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include "FakeRelayServer.h"

#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTest>

class ContactPhotoClientTest : public QObject
{
    Q_OBJECT

private slots:
    void fetchReturnsRawBytesOnSuccess();
    void fetchSendsAuthAsQueryParamsAndBuildsPathWithContactUid();
    void fetchUnauthorizedFrom401DegradesGracefullyToEmptyResult();
    void fetchOnTransportFailureDegradesGracefullyToEmptyResult();
};

void ContactPhotoClientTest::fetchReturnsRawBytesOnSuccess()
{
    const QByteArray photoBytes = QByteArrayLiteral("\xFF\xD8\xFF\xE0not-really-a-jpeg-but-bytes-are-bytes");
    FakeRelayServer fake(httpResponse(200, "OK", photoBytes, "image/jpeg"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactPhotoFetchResult result = client.fetch(serverBaseUrl, QStringLiteral("contact-1"), auth);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.photoBytes, photoBytes);
}

void ContactPhotoClientTest::fetchSendsAuthAsQueryParamsAndBuildsPathWithContactUid()
{
    FakeRelayServer fake(httpResponse(200, "OK", ""));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    client.fetch(serverBaseUrl, QStringLiteral("contact-42"), auth);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/contacts/contact-42/photo?"));
    QVERIFY(request.contains("sub=sub-9"));
    QVERIFY(request.contains("hash=hash-9"));
}

void ContactPhotoClientTest::fetchUnauthorizedFrom401DegradesGracefullyToEmptyResult()
{
    // Global Constraint (task-3-brief.md, same wording as task-2-brief.md's
    // rule for GroupsClient): ContactPhotoClient must degrade gracefully on
    // 401/error -- empty photoBytes, error set, never a crash -- since the
    // backend endpoint this depends on is an unverified external dependency
    // that may not be deployed.
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "Unauthorized\n"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactPhotoFetchResult result = client.fetch(serverBaseUrl, QStringLiteral("contact-1"), auth);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Unauthorized);
    QVERIFY(result.photoBytes.isEmpty());
}

void ContactPhotoClientTest::fetchOnTransportFailureDegradesGracefullyToEmptyResult()
{
    // Grab an ephemeral port, then close the listener immediately so
    // nothing is listening on it when the client connects -- same approach
    // as HttpClientTest::transportFailureWhenNothingListens(). Real
    // connection-refused transport failure, same "never crash"
    // degrade-gracefully requirement as the 401 case above.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost));
    const quint16 freePort = probe.serverPort();
    probe.close();

    QNetworkAccessManager manager;
    HttpClient http(manager);
    ContactPhotoClient client(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(freePort));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ContactPhotoFetchResult result = client.fetch(serverBaseUrl, QStringLiteral("contact-1"), auth);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Transport);
    QVERIFY(result.photoBytes.isEmpty());
}

QTEST_GUILESS_MAIN(ContactPhotoClientTest)
#include "ContactPhotoClientTest.moc"
