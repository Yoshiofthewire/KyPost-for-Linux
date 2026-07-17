#include "stores/ContactPhotoCache.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class ContactPhotoCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void cachedPathForReturnsEmptyWhenNothingStored();
    void storeThenCachedPathForRoundTripsBytes();
    void cachedPathForOnEmptyPhotoRefReturnsEmpty();
    void storeOnEmptyBytesReturnsEmptyAndWritesNothing();
    void storeOverwritesExistingFileForSameRef();
    void constructorCreatesCacheDirIfMissing();
};

void ContactPhotoCacheTest::cachedPathForReturnsEmptyWhenNothingStored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContactPhotoCache cache(dir.path());

    QVERIFY(cache.cachedPathFor(QStringLiteral("photo-ref-1")).isEmpty());
}

void ContactPhotoCacheTest::storeThenCachedPathForRoundTripsBytes()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContactPhotoCache cache(dir.path());

    const QByteArray bytes = QByteArrayLiteral("some-jpeg-bytes");
    const QString path = cache.store(QStringLiteral("photo-ref-1"), bytes);
    QVERIFY(!path.isEmpty());

    const QString cachedPath = cache.cachedPathFor(QStringLiteral("photo-ref-1"));
    QCOMPARE(cachedPath, path);

    QFile file(cachedPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), bytes);
}

void ContactPhotoCacheTest::cachedPathForOnEmptyPhotoRefReturnsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContactPhotoCache cache(dir.path());

    QVERIFY(cache.cachedPathFor(QString()).isEmpty());
}

void ContactPhotoCacheTest::storeOnEmptyBytesReturnsEmptyAndWritesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContactPhotoCache cache(dir.path());

    QVERIFY(cache.store(QStringLiteral("photo-ref-1"), QByteArray()).isEmpty());
    QVERIFY(cache.cachedPathFor(QStringLiteral("photo-ref-1")).isEmpty());
}

void ContactPhotoCacheTest::storeOverwritesExistingFileForSameRef()
{
    // photoRef is content-hashed/immutable per the doc, so this is a
    // defensive-behavior test, not a real-world expected path -- storing
    // different bytes under the same ref must still not crash or leave
    // stale bytes behind.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContactPhotoCache cache(dir.path());

    cache.store(QStringLiteral("photo-ref-1"), QByteArrayLiteral("first-version"));
    const QString path = cache.store(QStringLiteral("photo-ref-1"), QByteArrayLiteral("second-version"));
    QVERIFY(!path.isEmpty());

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArrayLiteral("second-version"));
}

void ContactPhotoCacheTest::constructorCreatesCacheDirIfMissing()
{
    QTemporaryDir parent;
    QVERIFY(parent.isValid());
    const QString cacheDir = parent.path() + QStringLiteral("/contact-photos");
    QVERIFY(!QFile::exists(cacheDir));

    ContactPhotoCache cache(cacheDir);
    QVERIFY(QFile::exists(cacheDir));

    // Also usable immediately -- proves mkpath actually ran before any
    // store()/cachedPathFor() call, not just that the directory happens to
    // exist.
    const QString path = cache.store(QStringLiteral("photo-ref-1"), QByteArrayLiteral("bytes"));
    QVERIFY(!path.isEmpty());
}

QTEST_GUILESS_MAIN(ContactPhotoCacheTest)
#include "ContactPhotoCacheTest.moc"
