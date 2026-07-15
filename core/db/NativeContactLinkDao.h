#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

// Tracks which local Contact (by uid) is linked to which native address-book
// item, per backend ('akonadi' | 'eds'). last_synced_hash/last_synced_at let
// a later sync task detect which side changed since the last successful sync
// -- this DAO only stores/retrieves that column, it doesn't compute hashes.
struct NativeContactLink
{
    int id = 0;
    QString localUid;
    QString backend;
    QString nativeItemId;
    QString nativeSourceId;
    QString lastSyncedHash;
    QString lastSyncedAt;

    bool operator==(const NativeContactLink&) const = default;
};

class NativeContactLinkDao
{
public:
    explicit NativeContactLinkDao(QSqlDatabase& db);

    bool insertOrReplace(const NativeContactLink& link); // keyed by (local_uid, backend)
    std::optional<NativeContactLink> findByLocalUid(const QString& localUid, const QString& backend) const;
    std::optional<NativeContactLink> findByNativeItemId(const QString& backend, const QString& nativeItemId) const;
    QVector<NativeContactLink> findAllForBackend(const QString& backend) const;
    bool deleteByLocalUid(const QString& localUid, const QString& backend);
    bool rekeyLocalUid(const QString& oldUid, const QString& newUid, const QString& backend);

private:
    QSqlDatabase& m_db;
};
