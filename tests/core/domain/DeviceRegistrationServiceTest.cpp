#include "domain/DeviceRegistrationService.h"
#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "net/HttpClient.h"
#include "net/NativeRegistrationClient.h"
#include "stores/SecureStoreFile.h"
#include "stores/SettingsStore.h"

#include "../net/FakeRelayServer.h"

#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QTest>

class DeviceRegistrationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void successfulPairPersistsPairingAndSettings();
    void unauthorizedPairLeavesStoresUntouched();
    void reregisterIfPairedSendsStoredCredentialsAndUpdatesDeviceId();
    void reregisterIfPairedWithNoPriorPairingMakesNoRequest();
    void reregisterIfPairedOn401LeavesStoredPairingUnchanged();

private:
    static PairingParams sampleParams(quint16 port);
};

PairingParams DeviceRegistrationServiceTest::sampleParams(quint16 port)
{
    PairingParams params;
    params.subscriberId = QStringLiteral("sub-1");
    params.subscriberHash = QStringLiteral("hash-1");
    params.serverBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(port);
    params.registrationUrl = QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(port);
    params.pairingToken = QStringLiteral("pair-tok");
    params.deviceName = QStringLiteral("My Linux Desktop");
    return params;
}

void DeviceRegistrationServiceTest::successfulPairPersistsPairingAndSettings()
{
    const QByteArray body = R"({"ok":true,"synced":true,"deviceId":"dev-1","devices":1,)"
                             R"("deliveryMode":"pull","pullEndpoint":"http://relay.example/api/notifications/native/pull",)"
                             R"("transport":"unifiedpush"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    DeviceRegistrationService service(client, pairingStore, settingsStore);

    const NativeRegistrationResult result =
        service.pair(sampleParams(fake.port()), QStringLiteral("https://push.example/endpoint"));

    QCOMPARE(result.outcome, RegistrationOutcome::Success);

    // Verify persistence via a second PairingStore instance over the same
    // SecureStoreFile directory, proving the write actually landed on disk
    // rather than merely being visible through the original instance.
    PairingStore verifyPairingStore(secureStore);
    const std::optional<DevicePairing> loaded = verifyPairingStore.load();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->subscriberId, QStringLiteral("sub-1"));
    QCOMPARE(loaded->subscriberHash, QStringLiteral("hash-1"));
    QCOMPARE(loaded->serverBaseUrl, sampleParams(fake.port()).serverBaseUrl);
    QCOMPARE(loaded->registrationUrl, sampleParams(fake.port()).registrationUrl);
    QCOMPARE(loaded->pairingToken, QStringLiteral("pair-tok"));
    QCOMPARE(loaded->deviceId, QStringLiteral("dev-1"));
    QCOMPARE(loaded->deviceName, QStringLiteral("My Linux Desktop"));

    QCOMPARE(settingsStore.deliveryMode(), QStringLiteral("pull"));
    QCOMPARE(settingsStore.pullEndpoint(), QStringLiteral("http://relay.example/api/notifications/native/pull"));
    QCOMPARE(settingsStore.transport(), QStringLiteral("unifiedpush"));
}

void DeviceRegistrationServiceTest::unauthorizedPairLeavesStoresUntouched()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "{}"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    DeviceRegistrationService service(client, pairingStore, settingsStore);

    const NativeRegistrationResult result =
        service.pair(sampleParams(fake.port()), QStringLiteral("https://push.example/endpoint"));

    QCOMPARE(result.outcome, RegistrationOutcome::Unauthorized);
    QVERIFY(!pairingStore.load().has_value());
    QVERIFY(settingsStore.deliveryMode().isEmpty());
}

void DeviceRegistrationServiceTest::reregisterIfPairedSendsStoredCredentialsAndUpdatesDeviceId()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    DevicePairing existing;
    existing.subscriberId = QStringLiteral("sub-existing");
    existing.subscriberHash = QStringLiteral("hash-existing");
    existing.serverBaseUrl = QStringLiteral("http://127.0.0.1:1");
    existing.registrationUrl = QStringLiteral("http://placeholder/api/notifications/native/register");
    existing.pairingToken = QStringLiteral("existing-token");
    existing.deviceId = QStringLiteral("old-device-id");
    existing.deviceName = QStringLiteral("Existing Desktop");
    QVERIFY(pairingStore.save(existing));

    const QByteArray body = R"({"ok":true,"synced":true,"deviceId":"new-device-id","devices":1,)"
                             R"("deliveryMode":"push","pullEndpoint":"http://relay.example/api/notifications/native/pull",)"
                             R"("transport":"unifiedpush"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    // registrationUrl in the stored pairing points nowhere real; the
    // service must use it verbatim, so re-point the stored pairing at the
    // fake server before triggering reregisterIfPaired().
    existing.registrationUrl = QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(fake.port());
    QVERIFY(pairingStore.save(existing));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);
    DeviceRegistrationService service(client, pairingStore, settingsStore);

    const std::optional<NativeRegistrationResult> result =
        service.reregisterIfPaired(QStringLiteral("https://push.example/endpoint"));

    QVERIFY(result.has_value());
    QCOMPARE(result->outcome, RegistrationOutcome::Success);

    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("subscriberId")).toString(), QStringLiteral("sub-existing"));
    QCOMPARE(sent.value(QStringLiteral("pairingToken")).toString(), QStringLiteral("existing-token"));

    const std::optional<DevicePairing> loaded = pairingStore.load();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->deviceId, QStringLiteral("new-device-id"));
}

void DeviceRegistrationServiceTest::reregisterIfPairedWithNoPriorPairingMakesNoRequest()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true})"));

    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);
    DeviceRegistrationService service(client, pairingStore, settingsStore);

    const std::optional<NativeRegistrationResult> result =
        service.reregisterIfPaired(QStringLiteral("https://push.example/endpoint"));

    QVERIFY(!result.has_value());
    QVERIFY(fake.receivedRequest().isEmpty());
}

void DeviceRegistrationServiceTest::reregisterIfPairedOn401LeavesStoredPairingUnchanged()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    FakeRelayServer fake(httpResponse(401, "Unauthorized", "{}"));

    DevicePairing existing;
    existing.subscriberId = QStringLiteral("sub-existing");
    existing.subscriberHash = QStringLiteral("hash-existing");
    existing.serverBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(fake.port());
    existing.registrationUrl = QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(fake.port());
    existing.pairingToken = QStringLiteral("existing-token");
    existing.deviceId = QStringLiteral("old-device-id");
    existing.deviceName = QStringLiteral("Existing Desktop");
    QVERIFY(pairingStore.save(existing));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);
    DeviceRegistrationService service(client, pairingStore, settingsStore);

    const std::optional<NativeRegistrationResult> result =
        service.reregisterIfPaired(QStringLiteral("https://push.example/endpoint"));

    QVERIFY(result.has_value());
    QCOMPARE(result->outcome, RegistrationOutcome::Unauthorized);

    const std::optional<DevicePairing> loaded = pairingStore.load();
    QVERIFY(loaded.has_value());
    QCOMPARE(*loaded, existing);
}

QTEST_GUILESS_MAIN(DeviceRegistrationServiceTest)
#include "DeviceRegistrationServiceTest.moc"
