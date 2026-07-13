#include "domain/PairingStore.h"
#include "stores/SecureStoreFile.h"

#include <QTemporaryDir>
#include <QTest>

class PairingStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void isPairedFalseBeforeAnySave();
    void saveThenLoadRoundTripsEveryField();
    void loadReturnsNulloptWhenSubMissingEvenIfOtherKeysExist();
    void clearThenLoadReturnsNullopt();
    void subscriberHashEmptyStringRoundTripsAsEmpty();

private:
    static DevicePairing samplePairing();
};

DevicePairing PairingStoreTest::samplePairing()
{
    DevicePairing pairing;
    pairing.subscriberId = QStringLiteral("subscriber-123");
    pairing.subscriberHash = QStringLiteral("deadbeef");
    pairing.serverBaseUrl = QStringLiteral("https://relay.example.com");
    pairing.registrationUrl = QStringLiteral("https://relay.example.com/api/notifications/native/register");
    pairing.pairingToken = QStringLiteral("pairing-token-abc");
    pairing.deviceId = QStringLiteral("device-1");
    pairing.deviceName = QStringLiteral("My Linux Desktop");
    return pairing;
}

void PairingStoreTest::isPairedFalseBeforeAnySave()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile secureStore(dir.path());
    PairingStore pairingStore(secureStore);

    QVERIFY(!pairingStore.isPaired());
    QVERIFY(!pairingStore.load().has_value());
}

void PairingStoreTest::saveThenLoadRoundTripsEveryField()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile secureStore(dir.path());
    PairingStore pairingStore(secureStore);

    const DevicePairing pairing = samplePairing();
    QVERIFY(pairingStore.save(pairing));

    const std::optional<DevicePairing> loaded = pairingStore.load();
    QVERIFY(loaded.has_value());
    QCOMPARE(*loaded, pairing);
    QVERIFY(pairingStore.isPaired());
}

void PairingStoreTest::loadReturnsNulloptWhenSubMissingEvenIfOtherKeysExist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile secureStore(dir.path());
    PairingStore pairingStore(secureStore);

    // Write the other six keys directly via the underlying SecureStoreFile,
    // skipping "sub", to confirm load() still treats this as unpaired.
    QVERIFY(secureStore.set(QStringLiteral("hash"), QStringLiteral("deadbeef")));
    QVERIFY(secureStore.set(QStringLiteral("pairing.serverBaseUrl"), QStringLiteral("https://relay.example.com")));
    QVERIFY(secureStore.set(QStringLiteral("pairing.registrationUrl"),
        QStringLiteral("https://relay.example.com/api/notifications/native/register")));
    QVERIFY(secureStore.set(QStringLiteral("pairing.pairingToken"), QStringLiteral("pairing-token-abc")));
    QVERIFY(secureStore.set(QStringLiteral("deviceId"), QStringLiteral("device-1")));
    QVERIFY(secureStore.set(QStringLiteral("pairing.deviceName"), QStringLiteral("My Linux Desktop")));

    QVERIFY(!pairingStore.load().has_value());
    QVERIFY(!pairingStore.isPaired());
}

void PairingStoreTest::clearThenLoadReturnsNullopt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile secureStore(dir.path());
    PairingStore pairingStore(secureStore);

    QVERIFY(pairingStore.save(samplePairing()));
    QVERIFY(pairingStore.isPaired());

    pairingStore.clear();

    QVERIFY(!pairingStore.load().has_value());
    QVERIFY(!pairingStore.isPaired());
}

void PairingStoreTest::subscriberHashEmptyStringRoundTripsAsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    SecureStoreFile secureStore(dir.path());
    PairingStore pairingStore(secureStore);

    DevicePairing pairing = samplePairing();
    pairing.subscriberHash = QString();
    QVERIFY(pairingStore.save(pairing));

    const std::optional<DevicePairing> loaded = pairingStore.load();
    QVERIFY(loaded.has_value());
    QVERIFY(loaded->subscriberHash.isEmpty());
    QCOMPARE(*loaded, pairing);
}

QTEST_GUILESS_MAIN(PairingStoreTest)
#include "PairingStoreTest.moc"
