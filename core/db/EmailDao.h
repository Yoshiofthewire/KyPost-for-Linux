#pragma once

#include "models/Email.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

class EmailDao
{
public:
    explicit EmailDao(QSqlDatabase& db);

    bool insertOrReplace(const Email& email);
    std::optional<Email> findById(const QString& messageId) const;
    QVector<Email> findByFolder(const QString& folder) const;
    QVector<Email> findAll() const;
    bool deleteById(const QString& messageId);
    bool deleteAll();

private:
    QSqlDatabase& m_db;
};
