#include "db/Database.h"
#include "db/PendingContactChangeDao.h"

#include <QTest>

class PendingContactChangeDaoTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void enqueueThenFindAllReturnsInInsertionOrder();
    void deleteByIdRemovesOnlyThatRow();
    void deleteAllClearsEverything();

private:
    Database m_db;
};

void PendingContactChangeDaoTest::init()
{
    QVERIFY(m_db.open(QStringLiteral(":memory:")));
}

void PendingContactChangeDaoTest::enqueueThenFindAllReturnsInInsertionOrder()
{
    PendingContactChangeDao dao(m_db.handle());

    const int id1 = dao.enqueue(QStringLiteral("uid-1"), QStringLiteral("{\"uid\":\"uid-1\"}"),
                                 QStringLiteral("2026-01-01T00:00:00Z"));
    const int id2 = dao.enqueue(QStringLiteral("uid-2"), QStringLiteral("{\"uid\":\"uid-2\"}"),
                                 QStringLiteral("2026-01-02T00:00:00Z"));
    QVERIFY(id1 >= 0);
    QVERIFY(id2 > id1);

    const QVector<PendingContactChangeRecord> all = dao.findAll();
    QCOMPARE(all.size(), 2);
    QCOMPARE(all.at(0).id, id1);
    QCOMPARE(all.at(0).contactUid, QStringLiteral("uid-1"));
    QCOMPARE(all.at(0).changeJson, QStringLiteral("{\"uid\":\"uid-1\"}"));
    QCOMPARE(all.at(0).createdAt, QStringLiteral("2026-01-01T00:00:00Z"));
    QCOMPARE(all.at(1).id, id2);
    QCOMPARE(all.at(1).contactUid, QStringLiteral("uid-2"));
}

void PendingContactChangeDaoTest::deleteByIdRemovesOnlyThatRow()
{
    PendingContactChangeDao dao(m_db.handle());

    const int id1 = dao.enqueue(QStringLiteral("uid-1"), QStringLiteral("{}"), QStringLiteral("t1"));
    const int id2 = dao.enqueue(QStringLiteral("uid-2"), QStringLiteral("{}"), QStringLiteral("t2"));

    QVERIFY(dao.deleteById(id1));

    const QVector<PendingContactChangeRecord> remaining = dao.findAll();
    QCOMPARE(remaining.size(), 1);
    QCOMPARE(remaining.at(0).id, id2);
}

void PendingContactChangeDaoTest::deleteAllClearsEverything()
{
    PendingContactChangeDao dao(m_db.handle());

    dao.enqueue(QStringLiteral("uid-1"), QStringLiteral("{}"), QStringLiteral("t1"));
    dao.enqueue(QStringLiteral("uid-2"), QStringLiteral("{}"), QStringLiteral("t2"));
    QCOMPARE(dao.findAll().size(), 2);

    QVERIFY(dao.deleteAll());
    QVERIFY(dao.findAll().isEmpty());
}

QTEST_GUILESS_MAIN(PendingContactChangeDaoTest)
#include "PendingContactChangeDaoTest.moc"
