#include "GroupDao.h"

#include <QSqlQuery>

namespace {

Group groupFromQuery(const QSqlQuery& query)
{
    Group group;
    group.id = query.value(QStringLiteral("id")).toString();
    group.name = query.value(QStringLiteral("name")).toString();
    group.rev = query.value(QStringLiteral("rev")).toLongLong();
    return group;
}

} // namespace

GroupDao::GroupDao(QSqlDatabase& db) : m_db(db)
{
}

bool GroupDao::insertOrReplace(const Group& group)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO groups (id, name, rev) VALUES (:id, :name, :rev)"));
    query.bindValue(QStringLiteral(":id"), group.id);
    query.bindValue(QStringLiteral(":name"), group.name);
    query.bindValue(QStringLiteral(":rev"), group.rev);
    return query.exec();
}

std::optional<Group> GroupDao::findById(const QString& id) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM groups WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return groupFromQuery(query);
}

QVector<Group> GroupDao::findAll() const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT * FROM groups")))
        return {};
    QVector<Group> results;
    while (query.next())
        results.append(groupFromQuery(query));
    return results;
}
