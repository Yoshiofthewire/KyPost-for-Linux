#include "db/Database.h"
#include "db/NativeContactLinkDao.h"

#include <QTest>

class NativeContactLinkDaoTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void roundTripsInsertAndFindByBothKeys();
    void findAllForBackendFiltersByBackend();
    void deleteByLocalUidRemovesRow();
    void rekeyLocalUidUpdatesKeyPreservingRestOfRow();
    void insertOrReplaceUpsertsOnLocalUidBackendKey();
    void insertOrReplaceUpsertsOnBackendNativeItemIdKey();

private:
    Database m_db;
};

namespace {

NativeContactLink makeLink(const QString& localUid, const QString& backend,
                            const QString& nativeItemId, const QString& nativeSourceId,
                            const QString& hash, const QString& syncedAt)
{
    NativeContactLink link;
    link.localUid = localUid;
    link.backend = backend;
    link.nativeItemId = nativeItemId;
    link.nativeSourceId = nativeSourceId;
    link.lastSyncedHash = hash;
    link.lastSyncedAt = syncedAt;
    return link;
}

} // namespace

void NativeContactLinkDaoTest::init()
{
    QVERIFY(m_db.open(QStringLiteral(":memory:")));
}

void NativeContactLinkDaoTest::roundTripsInsertAndFindByBothKeys()
{
    NativeContactLinkDao dao(m_db.handle());

    NativeContactLink link = makeLink(
        QStringLiteral("uid-1"), QStringLiteral("akonadi"), QStringLiteral("item-1"),
        QStringLiteral("source-1"), QStringLiteral("hash-1"),
        QStringLiteral("2026-01-01T00:00:00Z"));

    QVERIFY(dao.insertOrReplace(link));

    auto byLocalUid = dao.findByLocalUid(link.localUid, link.backend);
    QVERIFY(byLocalUid.has_value());
    QCOMPARE(byLocalUid->localUid, link.localUid);
    QCOMPARE(byLocalUid->backend, link.backend);
    QCOMPARE(byLocalUid->nativeItemId, link.nativeItemId);
    QCOMPARE(byLocalUid->nativeSourceId, link.nativeSourceId);
    QCOMPARE(byLocalUid->lastSyncedHash, link.lastSyncedHash);
    QCOMPARE(byLocalUid->lastSyncedAt, link.lastSyncedAt);
    QVERIFY(byLocalUid->id > 0);

    auto byNativeItemId = dao.findByNativeItemId(link.backend, link.nativeItemId);
    QVERIFY(byNativeItemId.has_value());
    QCOMPARE(byNativeItemId->localUid, link.localUid);
    QCOMPARE(byNativeItemId->id, byLocalUid->id);

    QVERIFY(!dao.findByLocalUid(QStringLiteral("uid-missing"), link.backend).has_value());
    QVERIFY(!dao.findByNativeItemId(link.backend, QStringLiteral("item-missing")).has_value());
    // Same native_item_id but different backend must not match.
    QVERIFY(!dao.findByNativeItemId(QStringLiteral("eds"), link.nativeItemId).has_value());
}

void NativeContactLinkDaoTest::findAllForBackendFiltersByBackend()
{
    NativeContactLinkDao dao(m_db.handle());

    QVERIFY(dao.insertOrReplace(makeLink(
        QStringLiteral("uid-a"), QStringLiteral("akonadi"), QStringLiteral("item-a"),
        QStringLiteral("source-a"), QStringLiteral("hash-a"), QStringLiteral("2026-01-01T00:00:00Z"))));
    QVERIFY(dao.insertOrReplace(makeLink(
        QStringLiteral("uid-b"), QStringLiteral("akonadi"), QStringLiteral("item-b"),
        QStringLiteral("source-b"), QStringLiteral("hash-b"), QStringLiteral("2026-01-02T00:00:00Z"))));
    QVERIFY(dao.insertOrReplace(makeLink(
        QStringLiteral("uid-c"), QStringLiteral("eds"), QStringLiteral("item-c"),
        QStringLiteral("source-c"), QStringLiteral("hash-c"), QStringLiteral("2026-01-03T00:00:00Z"))));

    const QVector<NativeContactLink> akonadiLinks = dao.findAllForBackend(QStringLiteral("akonadi"));
    QCOMPARE(akonadiLinks.size(), 2);

    const QVector<NativeContactLink> edsLinks = dao.findAllForBackend(QStringLiteral("eds"));
    QCOMPARE(edsLinks.size(), 1);
    QCOMPARE(edsLinks.first().localUid, QStringLiteral("uid-c"));

    QCOMPARE(dao.findAllForBackend(QStringLiteral("nonexistent")).size(), 0);
}

void NativeContactLinkDaoTest::deleteByLocalUidRemovesRow()
{
    NativeContactLinkDao dao(m_db.handle());

    NativeContactLink link = makeLink(
        QStringLiteral("uid-1"), QStringLiteral("akonadi"), QStringLiteral("item-1"),
        QStringLiteral("source-1"), QStringLiteral("hash-1"),
        QStringLiteral("2026-01-01T00:00:00Z"));
    QVERIFY(dao.insertOrReplace(link));

    QVERIFY(dao.deleteByLocalUid(link.localUid, link.backend));
    QVERIFY(!dao.findByLocalUid(link.localUid, link.backend).has_value());
}

void NativeContactLinkDaoTest::rekeyLocalUidUpdatesKeyPreservingRestOfRow()
{
    NativeContactLinkDao dao(m_db.handle());

    NativeContactLink link = makeLink(
        QStringLiteral("uid-old"), QStringLiteral("akonadi"), QStringLiteral("item-1"),
        QStringLiteral("source-1"), QStringLiteral("hash-1"),
        QStringLiteral("2026-01-01T00:00:00Z"));
    QVERIFY(dao.insertOrReplace(link));

    QVERIFY(dao.rekeyLocalUid(QStringLiteral("uid-old"), QStringLiteral("uid-new"), link.backend));

    QVERIFY(!dao.findByLocalUid(QStringLiteral("uid-old"), link.backend).has_value());

    auto rekeyed = dao.findByLocalUid(QStringLiteral("uid-new"), link.backend);
    QVERIFY(rekeyed.has_value());
    QCOMPARE(rekeyed->nativeItemId, link.nativeItemId);
    QCOMPARE(rekeyed->nativeSourceId, link.nativeSourceId);
    QCOMPARE(rekeyed->lastSyncedHash, link.lastSyncedHash);
    QCOMPARE(rekeyed->lastSyncedAt, link.lastSyncedAt);
}

void NativeContactLinkDaoTest::insertOrReplaceUpsertsOnLocalUidBackendKey()
{
    NativeContactLinkDao dao(m_db.handle());

    NativeContactLink link = makeLink(
        QStringLiteral("uid-1"), QStringLiteral("akonadi"), QStringLiteral("item-1"),
        QStringLiteral("source-1"), QStringLiteral("hash-1"),
        QStringLiteral("2026-01-01T00:00:00Z"));
    QVERIFY(dao.insertOrReplace(link));

    // Re-insert with the same (local_uid, backend) key but a different
    // native_item_id/hash -- must update in place, not fail or duplicate.
    NativeContactLink updated = link;
    updated.nativeItemId = QStringLiteral("item-1-renamed");
    updated.lastSyncedHash = QStringLiteral("hash-2");
    updated.lastSyncedAt = QStringLiteral("2026-01-05T00:00:00Z");
    QVERIFY(dao.insertOrReplace(updated));

    QCOMPARE(dao.findAllForBackend(link.backend).size(), 1);

    auto found = dao.findByLocalUid(link.localUid, link.backend);
    QVERIFY(found.has_value());
    QCOMPARE(found->nativeItemId, updated.nativeItemId);
    QCOMPARE(found->lastSyncedHash, updated.lastSyncedHash);
}

void NativeContactLinkDaoTest::insertOrReplaceUpsertsOnBackendNativeItemIdKey()
{
    NativeContactLinkDao dao(m_db.handle());

    NativeContactLink link = makeLink(
        QStringLiteral("uid-1"), QStringLiteral("akonadi"), QStringLiteral("item-1"),
        QStringLiteral("source-1"), QStringLiteral("hash-1"),
        QStringLiteral("2026-01-01T00:00:00Z"));
    QVERIFY(dao.insertOrReplace(link));

    // Re-insert with the same (backend, native_item_id) key but a different
    // local_uid -- must update in place, not fail or duplicate.
    NativeContactLink relinked = link;
    relinked.localUid = QStringLiteral("uid-2");
    relinked.lastSyncedHash = QStringLiteral("hash-3");
    QVERIFY(dao.insertOrReplace(relinked));

    QCOMPARE(dao.findAllForBackend(link.backend).size(), 1);

    auto found = dao.findByNativeItemId(link.backend, link.nativeItemId);
    QVERIFY(found.has_value());
    QCOMPARE(found->localUid, relinked.localUid);
    QCOMPARE(found->lastSyncedHash, relinked.lastSyncedHash);
}

QTEST_GUILESS_MAIN(NativeContactLinkDaoTest)
#include "NativeContactLinkDaoTest.moc"
