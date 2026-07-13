#pragma once

#include <QSqlDatabase>
#include <QString>

// Opens a SQLite connection (":memory:" or a real file path) and applies
// core/db/migrations/001_initial.sql idempotently, guarded by
// `PRAGMA user_version`. Each Database owns a uniquely-named QSqlDatabase
// connection (Qt requires unique connection names) and removes it on
// destruction.
class Database
{
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(const QString& path);
    QSqlDatabase& handle();

private:
    QSqlDatabase m_db;
    QString m_connectionName;
};
