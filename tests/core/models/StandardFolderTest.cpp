#include "models/StandardFolder.h"

#include <QTest>

class StandardFolderTest : public QObject
{
    Q_OBJECT

private slots:
    void wireNameRoundTripsForAllValues();
    void fromWireNameRejectsUnknown();
    void displayNameSplitsOnSlash();
    void displayNameSplitsOnDot();
    void displayNameHandlesPlainPath();
};

void StandardFolderTest::wireNameRoundTripsForAllValues()
{
    const QVector<QPair<StandardFolder, QString>> cases = {
        {StandardFolder::Inbox, QStringLiteral("INBOX")},
        {StandardFolder::Drafts, QStringLiteral("Drafts")},
        {StandardFolder::Junk, QStringLiteral("Junk")},
        {StandardFolder::Sent, QStringLiteral("Sent")},
        {StandardFolder::Trash, QStringLiteral("Trash")},
        {StandardFolder::Archive, QStringLiteral("Archive")},
    };

    for (const auto& [folder, wireName] : cases) {
        QCOMPARE(standardFolderWireName(folder), wireName);
        const std::optional<StandardFolder> roundTripped = standardFolderFromWireName(wireName);
        QVERIFY(roundTripped.has_value());
        QCOMPARE(*roundTripped, folder);
    }
}

void StandardFolderTest::fromWireNameRejectsUnknown()
{
    QVERIFY(!standardFolderFromWireName(QStringLiteral("NotAFolder")).has_value());
    QVERIFY(!standardFolderFromWireName(QString()).has_value());
}

void StandardFolderTest::displayNameSplitsOnSlash()
{
    QCOMPARE(standardFolderDisplayName(QStringLiteral("Archive/2024")), QStringLiteral("2024"));
}

void StandardFolderTest::displayNameSplitsOnDot()
{
    QCOMPARE(standardFolderDisplayName(QStringLiteral("INBOX.Old")), QStringLiteral("Old"));
}

void StandardFolderTest::displayNameHandlesPlainPath()
{
    QCOMPARE(standardFolderDisplayName(QStringLiteral("INBOX")), QStringLiteral("INBOX"));
}

QTEST_APPLESS_MAIN(StandardFolderTest)
#include "StandardFolderTest.moc"
