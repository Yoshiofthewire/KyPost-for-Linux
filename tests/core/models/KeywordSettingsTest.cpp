#include "models/KeywordSettings.h"

#include <QTest>

class KeywordSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultConstructs();
    void populatesAndCompares();
};

void KeywordSettingsTest::defaultConstructs()
{
    KeywordSettings settings;
    QVERIFY(settings.keyword.isEmpty());
    QVERIFY(!settings.visible);
}

void KeywordSettingsTest::populatesAndCompares()
{
    KeywordSettings settings;
    settings.keyword = QStringLiteral("urgent");
    settings.visible = true;

    KeywordSettings copy = settings;
    QCOMPARE(copy, settings);

    KeywordSettings assigned;
    assigned = settings;
    QCOMPARE(assigned, settings);

    KeywordSettings different = settings;
    different.visible = false;
    QVERIFY(different != settings);
}

QTEST_APPLESS_MAIN(KeywordSettingsTest)
#include "KeywordSettingsTest.moc"
