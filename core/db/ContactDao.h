#pragma once

#include "models/Contact.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

class ContactDao
{
public:
    explicit ContactDao(QSqlDatabase& db);

    bool insertOrReplace(const Contact& contact);
    std::optional<Contact> findById(const QString& uid) const;
    QVector<Contact> findAll() const;
    bool deleteById(const QString& uid);
    bool deleteAll();

private:
    QSqlDatabase& m_db;
};
