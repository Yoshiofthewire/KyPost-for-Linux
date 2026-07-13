#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

// No Folder model exists in core/models (Task 3 didn't define one) — the
// `folders` table is addressed by raw fields per the task-4 brief, so this
// DAO defines its own minimal read-side record type.
struct FolderRecord
{
    QString path;
    QString parent;
    bool deletable = false;
    QString sourceMode;

    bool operator==(const FolderRecord&) const = default;
};

class FolderDao
{
public:
    explicit FolderDao(QSqlDatabase& db);

    bool insertOrReplace(const QString& path, const QString& parent, bool deletable,
                          const QString& sourceMode);
    std::optional<FolderRecord> findByPath(const QString& path) const;
    QVector<FolderRecord> findByParent(const QString& parent) const;
    QVector<FolderRecord> findAll() const;
    bool deleteByPath(const QString& path);
    bool deleteAll();

private:
    QSqlDatabase& m_db;
};
