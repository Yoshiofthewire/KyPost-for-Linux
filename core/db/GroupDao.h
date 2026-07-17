#pragma once

#include "models/Group.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

// DAO over the `groups` table (see core/db/migrations/003_extended_contact_
// fields.sql), mirroring ContactDao's shape but for the much simpler
// {id, name, rev} Group struct -- no JSON-blob columns, no optional fields.
// Full-replace cache: GroupsRepository::refresh() calls insertOrReplace()
// for every row GroupsClient::fetch() returns, once per contact sync cycle;
// there is no delta/cursor tracking here (see task-2-brief.md).
class GroupDao
{
public:
    explicit GroupDao(QSqlDatabase& db);

    bool insertOrReplace(const Group& group);
    std::optional<Group> findById(const QString& id) const;
    QVector<Group> findAll() const;

private:
    QSqlDatabase& m_db;
};
