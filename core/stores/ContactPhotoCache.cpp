#include "stores/ContactPhotoCache.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

ContactPhotoCache::ContactPhotoCache(const QString& cacheDir)
    : m_dir(cacheDir)
{
    m_dir.mkpath(QStringLiteral("."));
}

QString ContactPhotoCache::fileNameFor(const QString& photoRef) const
{
    const QByteArray hash = QCryptographicHash::hash(photoRef.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

QString ContactPhotoCache::cachedPathFor(const QString& photoRef) const
{
    if (photoRef.isEmpty())
        return QString();

    const QString path = m_dir.filePath(fileNameFor(photoRef));
    return QFileInfo::exists(path) ? path : QString();
}

QString ContactPhotoCache::store(const QString& photoRef, const QByteArray& bytes) const
{
    if (photoRef.isEmpty() || bytes.isEmpty())
        return QString();

    const QString path = m_dir.filePath(fileNameFor(photoRef));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QString();

    const qint64 written = file.write(bytes);
    file.close();
    if (written != bytes.size()) {
        QFile::remove(path); // don't leave a truncated/corrupt cache entry behind
        return QString();
    }

    return path;
}
