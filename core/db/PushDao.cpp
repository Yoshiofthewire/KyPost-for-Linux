#include "PushDao.h"

#include <QSqlQuery>
#include <QVariant>

namespace {

PushRecord pushFromQuery(const QSqlQuery& query)
{
    PushRecord record;
    record.messageId = query.value(QStringLiteral("message_id")).toString();
    record.seq = query.value(QStringLiteral("seq")).toLongLong();
    record.receivedAt = query.value(QStringLiteral("received_at")).toString();
    record.consumed = query.value(QStringLiteral("consumed")).toInt() != 0;
    return record;
}

} // namespace

PushDao::PushDao(QSqlDatabase& db) : m_db(db)
{
}

bool PushDao::insertOrReplace(const QString& messageId, qint64 seq, const QString& receivedAt,
                               bool consumed)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO push_notifications (message_id, seq, received_at, consumed) "
        "VALUES (:message_id, :seq, :received_at, :consumed)"));
    query.bindValue(QStringLiteral(":message_id"), messageId);
    query.bindValue(QStringLiteral(":seq"), seq);
    query.bindValue(QStringLiteral(":received_at"), receivedAt);
    query.bindValue(QStringLiteral(":consumed"), consumed ? 1 : 0);
    return query.exec();
}

std::optional<PushRecord> PushDao::findById(const QString& messageId) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM push_notifications WHERE message_id = :message_id"));
    query.bindValue(QStringLiteral(":message_id"), messageId);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return pushFromQuery(query);
}

QVector<PushRecord> PushDao::findUnconsumed() const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT * FROM push_notifications WHERE consumed = 0")))
        return {};
    QVector<PushRecord> results;
    while (query.next())
        results.append(pushFromQuery(query));
    return results;
}

bool PushDao::markConsumed(const QString& messageId)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE push_notifications SET consumed = 1 WHERE message_id = :message_id"));
    query.bindValue(QStringLiteral(":message_id"), messageId);
    return query.exec();
}

bool PushDao::deleteAll()
{
    QSqlQuery query(m_db);
    return query.exec(QStringLiteral("DELETE FROM push_notifications"));
}
