#include "db/Database.h"
#include "db/PushDao.h"

#include <QTest>

class PushDaoTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void roundTripsInsertUpdateDelete();
    void findsUnconsumedAndMarksConsumed();

private:
    Database m_db;
};

void PushDaoTest::init()
{
    QVERIFY(m_db.open(QStringLiteral(":memory:")));
}

void PushDaoTest::roundTripsInsertUpdateDelete()
{
    PushDao dao(m_db.handle());

    QVERIFY(dao.insertOrReplace(QStringLiteral("msg-1"), 10, QStringLiteral("2026-01-01T00:00:00Z"),
                                 false));

    auto found = dao.findById(QStringLiteral("msg-1"));
    QVERIFY(found.has_value());
    QCOMPARE(found->messageId, QStringLiteral("msg-1"));
    QCOMPARE(found->seq, qint64(10));
    QCOMPARE(found->receivedAt, QStringLiteral("2026-01-01T00:00:00Z"));
    QCOMPARE(found->consumed, false);

    QVERIFY(dao.insertOrReplace(QStringLiteral("msg-1"), 11, QStringLiteral("2026-01-02T00:00:00Z"),
                                 true));
    auto updated = dao.findById(QStringLiteral("msg-1"));
    QVERIFY(updated.has_value());
    QCOMPARE(updated->seq, qint64(11));
    QCOMPARE(updated->consumed, true);

    QVERIFY(dao.deleteAll());
    QVERIFY(!dao.findById(QStringLiteral("msg-1")).has_value());
}

void PushDaoTest::findsUnconsumedAndMarksConsumed()
{
    PushDao dao(m_db.handle());

    QVERIFY(dao.insertOrReplace(QStringLiteral("msg-a"), 1, QStringLiteral("2026-01-01T00:00:00Z"),
                                 false));
    QVERIFY(dao.insertOrReplace(QStringLiteral("msg-b"), 2, QStringLiteral("2026-01-02T00:00:00Z"),
                                 false));

    QCOMPARE(dao.findUnconsumed().size(), 2);

    QVERIFY(dao.markConsumed(QStringLiteral("msg-a")));
    QCOMPARE(dao.findUnconsumed().size(), 1);
    QCOMPARE(dao.findUnconsumed().first().messageId, QStringLiteral("msg-b"));

    QVERIFY(dao.deleteAll());
    QCOMPARE(dao.findUnconsumed().size(), 0);
}

QTEST_GUILESS_MAIN(PushDaoTest)
#include "PushDaoTest.moc"
