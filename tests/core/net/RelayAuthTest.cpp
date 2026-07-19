#include "net/RelayAuth.h"

#include <QTest>

class RelayAuthTest : public QObject
{
    Q_OBJECT

private slots:
    void headerItemsReturnsDeviceIdAndSecretAsNamedHeaders();
};

void RelayAuthTest::headerItemsReturnsDeviceIdAndSecretAsNamedHeaders()
{
    const RelayAuth auth{ QStringLiteral("device-1"), QStringLiteral("secret-1") };

    const QList<QPair<QString, QString>> items = auth.headerItems();

    QCOMPARE(items.size(), 2);
    QCOMPARE(items.at(0).first, QStringLiteral("X-Kypost-Device-Id"));
    QCOMPARE(items.at(0).second, QStringLiteral("device-1"));
    QCOMPARE(items.at(1).first, QStringLiteral("X-Kypost-Device-Secret"));
    QCOMPARE(items.at(1).second, QStringLiteral("secret-1"));
}

QTEST_GUILESS_MAIN(RelayAuthTest)
#include "RelayAuthTest.moc"
