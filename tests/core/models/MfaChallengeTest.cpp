#include "models/MfaChallenge.h"

#include <QTest>

class MfaChallengeTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructs();
    void populatesAndCompares();
};

void MfaChallengeTest::defaultConstructs()
{
    MfaChallenge challenge;
    QVERIFY(challenge.challengeId.isEmpty());
    QVERIFY(challenge.receivedAt.isEmpty());
}

void MfaChallengeTest::populatesAndCompares()
{
    MfaChallenge challenge;
    challenge.challengeId = QStringLiteral("chal-1");
    challenge.receivedAt = QStringLiteral("2026-07-12T10:00:00Z");

    MfaChallenge copy = challenge;
    QCOMPARE(copy, challenge);

    MfaChallenge assigned;
    assigned = challenge;
    QCOMPARE(assigned, challenge);

    MfaChallenge different = challenge;
    different.challengeId = QStringLiteral("chal-2");
    QVERIFY(different != challenge);
}

QTEST_APPLESS_MAIN(MfaChallengeTest)
#include "MfaChallengeTest.moc"
