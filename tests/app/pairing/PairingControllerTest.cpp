#include "pairing/PairingController.h"

#include "domain/DeviceRegistrationService.h"
#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "net/HttpClient.h"
#include "net/NativeRegistrationClient.h"
#include "stores/SecureStoreFile.h"
#include "stores/SettingsStore.h"

#include "../../core/net/FakeRelayServer.h"

#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QUrlQuery>

class PairingControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void pairFromDeepLinkHappyPathPairsAndPersists();
    void pairFromDeepLinkSendsDeviceTokenWhenSet();
    void pairFromDeepLinkDerivesRegistrationUrlFromSrvWhenRegOmitted();
    void pairFromDeepLinkAllowsPresentButEmptyHashValue();
    void pairFromDeepLinkMissingRequiredParam_data();
    void pairFromDeepLinkMissingRequiredParam();
    void pairFromDeepLinkRejectsNonNativePairHost();
    void pairFromPastedLinkRejectsNonLinkTextWithNoNetworkCall();
    void refreshFromStoreReflectsPreSeededPairingStoreAndRemovePairingClears();
    void resetReturnsToIdleAfterFailure();

private:
    // Builds a llamalabels://native-pair?... link from a param map, letting
    // callers omit keys to exercise the missing-required-param path.
    static QUrl buildLink(const QMap<QString, QString>& params);
};

QUrl PairingControllerTest::buildLink(const QMap<QString, QString>& params)
{
    QUrl url;
    url.setScheme(QStringLiteral("llamalabels"));
    url.setHost(QStringLiteral("native-pair"));
    QUrlQuery query;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        query.addQueryItem(it.key(), it.value());
    url.setQuery(query);
    return url;
}

void PairingControllerTest::pairFromDeepLinkHappyPathPairsAndPersists()
{
    const QByteArray body = R"({"ok":true,"synced":true,"deviceId":"dev-1","devices":1,)"
                             R"("deliveryMode":"pull","pullEndpoint":"http://relay.example/api/notifications/native/pull",)"
                             R"("transport":"unifiedpush"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));

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

    PairingController controller(service, pairingStore, settingsStore);
    QVERIFY(!controller.isPaired());
    QCOMPARE(controller.pairingState(), QStringLiteral("idle"));

    QSignalSpy pairingChangedSpy(&controller, &PairingController::pairingChanged);
    QSignalSpy stateChangedSpy(&controller, &PairingController::pairingStateChanged);

    const QString serverBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(fake.port());
    const QString registrationUrl = serverBaseUrl + QStringLiteral("/api/notifications/native/register");

    QMap<QString, QString> params;
    params[QStringLiteral("sub")] = QStringLiteral("sub-1");
    params[QStringLiteral("hash")] = QStringLiteral("hash-1");
    params[QStringLiteral("srv")] = serverBaseUrl;
    params[QStringLiteral("pt")] = QStringLiteral("pair-tok");
    params[QStringLiteral("reg")] = registrationUrl;

    QVERIFY(controller.pairFromDeepLink(buildLink(params)));

    QCOMPARE(controller.pairingState(), QStringLiteral("paired"));
    QVERIFY(controller.pairingError().isEmpty());
    QVERIFY(controller.isPaired());
    QCOMPARE(controller.deviceId(), QStringLiteral("dev-1"));
    QCOMPARE(controller.pairedServerHost(), QStringLiteral("127.0.0.1"));
    // Task 39: deliveryMode/transport read straight through SettingsStore,
    // written by DeviceRegistrationService::pair() from this same response
    // body ("deliveryMode":"pull","transport":"unifiedpush" above).
    QCOMPARE(controller.deliveryMode(), QStringLiteral("pull"));
    QCOMPARE(controller.transport(), QStringLiteral("unifiedpush"));
    // Nothing in this codebase writes pushServerBaseUrl yet (see
    // PairingController.h's doc comment) -- still SettingsStore's baked-in
    // default after a successful pair.
    QCOMPARE(controller.pushServerBaseUrl(), QStringLiteral("https://ntfy.sh"));
    // "working" then "paired" -- at least two transitions.
    QVERIFY(stateChangedSpy.count() >= 2);
    QVERIFY(pairingChangedSpy.count() >= 1);

    const std::optional<DevicePairing> loaded = pairingStore.load();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->subscriberId, QStringLiteral("sub-1"));
    QCOMPARE(loaded->subscriberHash, QStringLiteral("hash-1"));
    QCOMPARE(loaded->registrationUrl, registrationUrl);
    QCOMPARE(loaded->deviceId, QStringLiteral("dev-1"));

    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("subscriberId")).toString(), QStringLiteral("sub-1"));
    QCOMPARE(sent.value(QStringLiteral("pairingToken")).toString(), QStringLiteral("pair-tok"));
    // This test never calls setDeviceToken(), so m_deviceToken stays at its
    // default-constructed empty QString() -- verifies the no-endpoint-yet
    // case (e.g. pairing completes before UnifiedPushConnector has ever
    // reported an endpoint). See pairFromDeepLinkSendsDeviceTokenWhenSet
    // below for the real-endpoint case.
    QCOMPARE(sent.value(QStringLiteral("deviceToken")).toString(), QString());
}

void PairingControllerTest::pairFromDeepLinkSendsDeviceTokenWhenSet()
{
    // Task 43 regression guard: when setDeviceToken() has been called (as
    // main.cpp does whenever UnifiedPushConnector reports an endpoint,
    // including once immediately after pushConnector's construction --
    // see PairingController.h's class doc comment), pairFromParsedParams()
    // must send that value as deviceToken rather than QString(). Reverting
    // the Task 43 fix (passing QString() unconditionally instead of
    // m_deviceToken) would fail this test while leaving
    // pairFromDeepLinkHappyPathPairsAndPersists above green.
    const QByteArray body = R"({"ok":true,"synced":true,"deviceId":"dev-4","devices":1,)"
                             R"("deliveryMode":"pull","pullEndpoint":"http://relay.example/api/notifications/native/pull",)"
                             R"("transport":"unifiedpush"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));

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

    PairingController controller(service, pairingStore, settingsStore);
    controller.setDeviceToken(QStringLiteral("some-real-endpoint"));

    const QString serverBaseUrl = QStringLiteral("http://127.0.0.1:%1").arg(fake.port());
    const QString registrationUrl = serverBaseUrl + QStringLiteral("/api/notifications/native/register");

    QMap<QString, QString> params;
    params[QStringLiteral("sub")] = QStringLiteral("sub-4");
    params[QStringLiteral("hash")] = QStringLiteral("hash-4");
    params[QStringLiteral("srv")] = serverBaseUrl;
    params[QStringLiteral("pt")] = QStringLiteral("pair-tok-4");
    params[QStringLiteral("reg")] = registrationUrl;

    QVERIFY(controller.pairFromDeepLink(buildLink(params)));
    QCOMPARE(controller.pairingState(), QStringLiteral("paired"));

    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("deviceToken")).toString(), QStringLiteral("some-real-endpoint"));
}

void PairingControllerTest::pairFromDeepLinkDerivesRegistrationUrlFromSrvWhenRegOmitted()
{
    const QByteArray body = R"({"ok":true,"synced":true,"deviceId":"dev-2","devices":1,)"
                             R"("deliveryMode":"pull","pullEndpoint":"http://relay.example/api/notifications/native/pull",)"
                             R"("transport":"unifiedpush"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));

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

    PairingController controller(service, pairingStore, settingsStore);

    // Trailing slash on srv exercises the strip-trailing-slash rule too.
    const QString serverBaseUrl = QStringLiteral("http://127.0.0.1:%1/").arg(fake.port());

    QMap<QString, QString> params;
    params[QStringLiteral("sub")] = QStringLiteral("sub-2");
    params[QStringLiteral("hash")] = QStringLiteral("hash-2");
    params[QStringLiteral("srv")] = serverBaseUrl;
    params[QStringLiteral("pt")] = QStringLiteral("pair-tok-2");
    // reg deliberately omitted.

    QVERIFY(controller.pairFromDeepLink(buildLink(params)));

    const std::optional<DevicePairing> loaded = pairingStore.load();
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->registrationUrl,
             QStringLiteral("http://127.0.0.1:%1/api/notifications/native/register").arg(fake.port()));

    // The derived path is what the request actually hit, not just what got
    // persisted.
    QVERIFY(fake.receivedRequest().contains("POST /api/notifications/native/register HTTP/1.1"));
}

void PairingControllerTest::pairFromDeepLinkAllowsPresentButEmptyHashValue()
{
    // subscriberHash "may be empty" per DevicePairing's own doc comment --
    // the deep link's "hash" query key must be present, but an empty value
    // is a legitimate "no hash" pairing, not a parse failure.
    const QByteArray body = R"({"ok":true,"synced":true,"deviceId":"dev-3","devices":1,)"
                             R"("deliveryMode":"pull","pullEndpoint":"http://relay.example/api/notifications/native/pull",)"
                             R"("transport":"unifiedpush"})";
    FakeRelayServer fake(httpResponse(200, "OK", body));

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

    PairingController controller(service, pairingStore, settingsStore);

    QMap<QString, QString> params;
    params[QStringLiteral("sub")] = QStringLiteral("sub-3");
    params[QStringLiteral("hash")] = QString(); // present, empty
    params[QStringLiteral("srv")] = QStringLiteral("http://127.0.0.1:%1").arg(fake.port());
    params[QStringLiteral("pt")] = QStringLiteral("pair-tok-3");

    QVERIFY(controller.pairFromDeepLink(buildLink(params)));
    QCOMPARE(controller.pairingState(), QStringLiteral("paired"));

    const std::optional<DevicePairing> loaded = pairingStore.load();
    QVERIFY(loaded.has_value());
    QVERIFY(loaded->subscriberHash.isEmpty());
}

void PairingControllerTest::pairFromDeepLinkMissingRequiredParam_data()
{
    QTest::addColumn<QString>("omittedKey");
    QTest::newRow("sub missing") << QStringLiteral("sub");
    QTest::newRow("hash key missing entirely") << QStringLiteral("hash");
    QTest::newRow("srv missing") << QStringLiteral("srv");
    QTest::newRow("pt missing") << QStringLiteral("pt");
}

void PairingControllerTest::pairFromDeepLinkMissingRequiredParam()
{
    QFETCH(QString, omittedKey);

    // Response would signal success if hit -- the test only passes if it's
    // never hit at all.
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"deviceId":"should-not-be-used"})"));

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

    PairingController controller(service, pairingStore, settingsStore);

    QMap<QString, QString> params;
    params[QStringLiteral("sub")] = QStringLiteral("sub-x");
    params[QStringLiteral("hash")] = QStringLiteral("hash-x");
    params[QStringLiteral("srv")] = QStringLiteral("http://127.0.0.1:%1").arg(fake.port());
    params[QStringLiteral("pt")] = QStringLiteral("pair-tok-x");
    params.remove(omittedKey);

    QSignalSpy stateChangedSpy(&controller, &PairingController::pairingStateChanged);

    QVERIFY(!controller.pairFromDeepLink(buildLink(params)));

    QCOMPARE(controller.pairingState(), QStringLiteral("failed"));
    QVERIFY(!controller.pairingError().isEmpty());
    QVERIFY(!controller.isPaired());
    QCOMPARE(stateChangedSpy.count(), 1); // idle -> failed directly, no "working" in between
    QVERIFY(fake.receivedRequest().isEmpty()); // zero network calls
    QVERIFY(!pairingStore.load().has_value());
}

void PairingControllerTest::pairFromDeepLinkRejectsNonNativePairHost()
{
    // llamalabels://desktop-pair is explicitly out of scope per Phase 6
    // global constraint 6 -- must be treated as unrecognized, not routed.
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

    PairingController controller(service, pairingStore, settingsStore);

    QUrl link;
    link.setScheme(QStringLiteral("llamalabels"));
    link.setHost(QStringLiteral("desktop-pair"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("sub"), QStringLiteral("sub-1"));
    query.addQueryItem(QStringLiteral("hash"), QStringLiteral("hash-1"));
    query.addQueryItem(QStringLiteral("srv"), QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    query.addQueryItem(QStringLiteral("pt"), QStringLiteral("pair-tok"));
    link.setQuery(query);

    QVERIFY(!controller.pairFromDeepLink(link));
    QCOMPARE(controller.pairingState(), QStringLiteral("failed"));
    QVERIFY(fake.receivedRequest().isEmpty());
}

void PairingControllerTest::pairFromPastedLinkRejectsNonLinkTextWithNoNetworkCall()
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

    PairingController controller(service, pairingStore, settingsStore);

    QVERIFY(!controller.pairFromPastedLink(QStringLiteral("this is not a pairing link")));
    QCOMPARE(controller.pairingState(), QStringLiteral("failed"));
    QVERIFY(fake.receivedRequest().isEmpty());
}

void PairingControllerTest::refreshFromStoreReflectsPreSeededPairingStoreAndRemovePairingClears()
{
    QTemporaryDir secureDir;
    QVERIFY(secureDir.isValid());
    SecureStoreFile secureStore(secureDir.path());
    PairingStore pairingStore(secureStore);

    DevicePairing pairing;
    pairing.subscriberId = QStringLiteral("sub-seed");
    pairing.subscriberHash = QStringLiteral("hash-seed");
    pairing.serverBaseUrl = QStringLiteral("https://relay.example.com:8443");
    pairing.registrationUrl = QStringLiteral("https://relay.example.com:8443/api/notifications/native/register");
    pairing.pairingToken = QStringLiteral("tok-seed");
    pairing.deviceId = QStringLiteral("dev-seed");
    pairing.deviceName = QStringLiteral("Seeded Device");
    QVERIFY(pairingStore.save(pairing));

    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    SettingsStore settingsStore(settingsDir.filePath(QStringLiteral("settings.ini")));

    QNetworkAccessManager manager;
    HttpClient http(manager);
    NativeRegistrationClient client(http);
    DeviceRegistrationService service(client, pairingStore, settingsStore);

    // Construction alone must reflect the pre-seeded pairing -- see
    // PairingController's constructor comment; no explicit refreshFromStore()
    // call needed here.
    PairingController controller(service, pairingStore, settingsStore);

    QVERIFY(controller.isPaired());
    QCOMPARE(controller.deviceId(), QStringLiteral("dev-seed"));
    QCOMPARE(controller.pairedServerHost(), QStringLiteral("relay.example.com"));
    // Task 39: this seed only touches PairingStore, never
    // DeviceRegistrationService::pair() -- SettingsStore's delivery fields
    // stay at their "never registered" empty default regardless of
    // isPaired, matching Settings.qml's Notifications pane "Not yet
    // registered" fallback.
    QVERIFY(controller.deliveryMode().isEmpty());
    QVERIFY(controller.transport().isEmpty());

    controller.removePairing();

    QVERIFY(!controller.isPaired());
    QVERIFY(controller.deviceId().isEmpty());
    QVERIFY(controller.pairedServerHost().isEmpty());
    QVERIFY(!pairingStore.load().has_value());
}

void PairingControllerTest::resetReturnsToIdleAfterFailure()
{
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

    PairingController controller(service, pairingStore, settingsStore);

    QVERIFY(!controller.pairFromPastedLink(QStringLiteral("not a link")));
    QCOMPARE(controller.pairingState(), QStringLiteral("failed"));

    controller.reset();

    QCOMPARE(controller.pairingState(), QStringLiteral("idle"));
    QVERIFY(controller.pairingError().isEmpty());
}

QTEST_GUILESS_MAIN(PairingControllerTest)
#include "PairingControllerTest.moc"
