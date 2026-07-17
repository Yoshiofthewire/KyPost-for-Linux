#include "db/GroupDao.h"

#include "db/Database.h"
#include "models/Group.h"

#include <QTest>

class GroupDaoTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void roundTripsInsertAndUpdate();
    void findByIdReturnsNulloptWhenAbsent();
    void findAllReturnsEveryRow();

private:
    Database m_db;
};

void GroupDaoTest::init()
{
    QVERIFY(m_db.open(QStringLiteral(":memory:")));
}

void GroupDaoTest::roundTripsInsertAndUpdate()
{
    GroupDao dao(m_db.handle());

    Group group;
    group.id = QStringLiteral("group-1");
    group.name = QStringLiteral("Family");
    group.rev = 1;

    QVERIFY(dao.insertOrReplace(group));

    auto found = dao.findById(group.id);
    QVERIFY(found.has_value());
    QCOMPARE(*found, group);

    // insertOrReplace() on an existing id updates in place, not a second row.
    Group updated = group;
    updated.name = QStringLiteral("Immediate Family");
    updated.rev = 2;
    QVERIFY(dao.insertOrReplace(updated));

    QCOMPARE(dao.findAll().size(), 1);
    auto refetched = dao.findById(group.id);
    QVERIFY(refetched.has_value());
    QCOMPARE(*refetched, updated);
}

void GroupDaoTest::findByIdReturnsNulloptWhenAbsent()
{
    GroupDao dao(m_db.handle());
    QVERIFY(!dao.findById(QStringLiteral("does-not-exist")).has_value());
}

void GroupDaoTest::findAllReturnsEveryRow()
{
    GroupDao dao(m_db.handle());

    Group group1;
    group1.id = QStringLiteral("group-a");
    group1.name = QStringLiteral("Work");
    group1.rev = 1;
    QVERIFY(dao.insertOrReplace(group1));

    Group group2;
    group2.id = QStringLiteral("group-b");
    group2.name = QStringLiteral("Friends");
    group2.rev = 3;
    QVERIFY(dao.insertOrReplace(group2));

    const QVector<Group> all = dao.findAll();
    QCOMPARE(all.size(), 2);
}

QTEST_GUILESS_MAIN(GroupDaoTest)
#include "GroupDaoTest.moc"
