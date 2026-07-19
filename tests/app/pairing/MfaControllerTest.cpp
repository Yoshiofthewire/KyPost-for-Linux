#include "pairing/MfaController.h"

#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "net/HttpClient.h"
#include "net/MfaResponseClient.h"
#include "stores/SecureStoreFile.h"

#include "../../core/net/FakeRelayServer.h"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class MfaControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void respondNotPairedShortCircuitsWithNoNetworkCall();
    void respondSuccessSendsStoredCredentialsAndSetsDone();
    void respondRejectedWithStatusDistinguishesAlreadyResolved();
    void respondFailureSetsDetailMessage();
    void resetReturnsToIdle();

private:
    static void savePairing(PairingStore& pairingStore, quint16 port);
};

void MfaControllerTest::savePairing(PairingStore& pairingStore, quint16 port)
{
    DevicePairing pairing;
    pairing.subscriberId = QStringLiteral("sub-1");
    pairing.serverBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(port);
    pairing.registrationUrl = pairing.serverBaseUrl + QStringLiteral("/api/notifications/native/register");
    pairing.pairingToken = QStringLiteral("pair-tok");
    pairing.deviceId = QStringLiteral("dev-1");
    pairing.deviceName = QStringLiteral("Test Device");
    pairing.deviceSecret = QStringLiteral("secret-1");
    QVERIFY(pairingStore.save(pairing));
}

void MfaControllerTest::respondNotPairedShortCircuitsWithNoNetworkCall()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"status":"approved"})"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // never saved -- not paired

    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    MfaController controller(client, pairingStore);
    QSignalSpy stateSpy(&controller, &MfaController::respondStateChanged);

    controller.respond(QStringLiteral("chal-1"), true);

    QCOMPARE(controller.respondState(), QStringLiteral("failed"));
    QCOMPARE(controller.resultMessage(), QStringLiteral("Not paired"));
    QCOMPARE(stateSpy.count(), 1); // idle -> failed directly, no "sending" in between
    QVERIFY(fake.receivedRequest().isEmpty());
}

void MfaControllerTest::respondSuccessSendsStoredCredentialsAndSetsDone()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"status":"approved"})"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    MfaController controller(client, pairingStore);
    QSignalSpy stateSpy(&controller, &MfaController::respondStateChanged);

    controller.respond(QStringLiteral("chal-42"), true);

    QCOMPARE(controller.respondState(), QStringLiteral("done"));
    QCOMPARE(controller.resultMessage(), QStringLiteral("Approved"));
    QVERIFY(stateSpy.count() >= 2); // sending -> done

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("POST /api/mfa/push/respond HTTP/1.1"));
    QVERIFY(request.contains("X-Kypost-Device-Id: dev-1"));
    QVERIFY(request.contains("X-Kypost-Device-Secret: secret-1"));
    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("challengeId")).toString(), QStringLiteral("chal-42"));
    QVERIFY(!sent.contains(QStringLiteral("subscriberId")));
    QVERIFY(!sent.contains(QStringLiteral("subscriberHash")));
    QVERIFY(!sent.contains(QStringLiteral("deviceId")));
    QCOMPARE(sent.value(QStringLiteral("approve")).toBool(), true);
}

void MfaControllerTest::respondRejectedWithStatusDistinguishesAlreadyResolved()
{
    FakeRelayServer fake(httpResponse(409, "Conflict", R"({"error":"already resolved","status":"denied"})"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    MfaController controller(client, pairingStore);
    controller.respond(QStringLiteral("chal-1"), false);

    QCOMPARE(controller.respondState(), QStringLiteral("failed"));
    QVERIFY(controller.resultMessage().contains(QStringLiteral("already resolved")));
    QVERIFY(controller.resultMessage().contains(QStringLiteral("denied")));
}

void MfaControllerTest::respondFailureSetsDetailMessage()
{
    FakeRelayServer fake(httpResponse(500, "Internal Server Error", "boom"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);
    savePairing(pairingStore, fake.port());

    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    MfaController controller(client, pairingStore);
    controller.respond(QStringLiteral("chal-1"), true);

    QCOMPARE(controller.respondState(), QStringLiteral("failed"));
    QVERIFY(!controller.resultMessage().isEmpty());
}

void MfaControllerTest::resetReturnsToIdle()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore); // not paired

    QNetworkAccessManager manager;
    HttpClient http(manager);
    MfaResponseClient client(http);

    MfaController controller(client, pairingStore);
    controller.respond(QStringLiteral("chal-1"), true);
    QCOMPARE(controller.respondState(), QStringLiteral("failed"));

    controller.reset();

    QCOMPARE(controller.respondState(), QStringLiteral("idle"));
    QVERIFY(controller.resultMessage().isEmpty());
}

QTEST_GUILESS_MAIN(MfaControllerTest)
#include "MfaControllerTest.moc"
