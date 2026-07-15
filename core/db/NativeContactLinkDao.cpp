#include "NativeContactLinkDao.h"

#include <QSqlQuery>
#include <QVariant>

namespace {

NativeContactLink linkFromQuery(const QSqlQuery& query)
{
    NativeContactLink link;
    link.id = query.value(QStringLiteral("id")).toInt();
    link.localUid = query.value(QStringLiteral("local_uid")).toString();
    link.backend = query.value(QStringLiteral("backend")).toString();
    link.nativeItemId = query.value(QStringLiteral("native_item_id")).toString();
    link.nativeSourceId = query.value(QStringLiteral("native_source_id")).toString();
    link.lastSyncedHash = query.value(QStringLiteral("last_synced_hash")).toString();
    link.lastSyncedAt = query.value(QStringLiteral("last_synced_at")).toString();
    return link;
}

} // namespace

NativeContactLinkDao::NativeContactLinkDao(QSqlDatabase& db) : m_db(db)
{
}

bool NativeContactLinkDao::insertOrReplace(const NativeContactLink& link)
{
    // id omitted so SQLite assigns a fresh one on conflict (OR REPLACE
    // deletes the conflicting row on either UNIQUE key, then inserts).
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO native_contact_links "
        "(local_uid, backend, native_item_id, native_source_id, last_synced_hash, last_synced_at) "
        "VALUES (:local_uid, :backend, :native_item_id, :native_source_id, :last_synced_hash, "
        ":last_synced_at)"));
    query.bindValue(QStringLiteral(":local_uid"), link.localUid);
    query.bindValue(QStringLiteral(":backend"), link.backend);
    query.bindValue(QStringLiteral(":native_item_id"), link.nativeItemId);
    query.bindValue(QStringLiteral(":native_source_id"), link.nativeSourceId);
    query.bindValue(QStringLiteral(":last_synced_hash"), link.lastSyncedHash);
    query.bindValue(QStringLiteral(":last_synced_at"), link.lastSyncedAt);
    return query.exec();
}

std::optional<NativeContactLink> NativeContactLinkDao::findByLocalUid(const QString& localUid,
                                                                       const QString& backend) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT * FROM native_contact_links WHERE local_uid = :local_uid AND backend = :backend"));
    query.bindValue(QStringLiteral(":local_uid"), localUid);
    query.bindValue(QStringLiteral(":backend"), backend);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return linkFromQuery(query);
}

std::optional<NativeContactLink> NativeContactLinkDao::findByNativeItemId(
    const QString& backend, const QString& nativeItemId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT * FROM native_contact_links WHERE backend = :backend AND native_item_id = :native_item_id"));
    query.bindValue(QStringLiteral(":backend"), backend);
    query.bindValue(QStringLiteral(":native_item_id"), nativeItemId);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return linkFromQuery(query);
}

QVector<NativeContactLink> NativeContactLinkDao::findAllForBackend(const QString& backend) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM native_contact_links WHERE backend = :backend"));
    query.bindValue(QStringLiteral(":backend"), backend);
    if (!query.exec())
        return {};
    QVector<NativeContactLink> results;
    while (query.next())
        results.append(linkFromQuery(query));
    return results;
}

bool NativeContactLinkDao::deleteByLocalUid(const QString& localUid, const QString& backend)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "DELETE FROM native_contact_links WHERE local_uid = :local_uid AND backend = :backend"));
    query.bindValue(QStringLiteral(":local_uid"), localUid);
    query.bindValue(QStringLiteral(":backend"), backend);
    return query.exec();
}

bool NativeContactLinkDao::rekeyLocalUid(const QString& oldUid, const QString& newUid,
                                          const QString& backend)
{
    // Used when a queued local create's temp UUID is replaced by the real
    // server uid after a successful push (see PendingContactChangeRecord)
    // -- the native link must follow the contact to its new identity.
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE native_contact_links SET local_uid = :new_uid "
        "WHERE local_uid = :old_uid AND backend = :backend"));
    query.bindValue(QStringLiteral(":new_uid"), newUid);
    query.bindValue(QStringLiteral(":old_uid"), oldUid);
    query.bindValue(QStringLiteral(":backend"), backend);
    return query.exec();
}
