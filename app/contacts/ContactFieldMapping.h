#pragma once

#include "models/Contact.h"

#include <QVariantList>
#include <QVariantMap>
#include <QVector>

// QVariantMap encoders for Contact's struct-entry fields, shared between
// ContactsController (read/write, full contact editing) and PgpQrController
// (read-only, scanned-card display) so both stop maintaining their own
// copies of the same field lists.

inline QVariantMap emailEntryToMap(const ContactEmailEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("value")] = entry.value;
    return map;
}

inline QVariantMap phoneEntryToMap(const ContactPhoneEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("value")] = entry.value;
    return map;
}

inline QVariantMap addressEntryToMap(const ContactAddressEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("street")] = entry.street.value_or(QString());
    map[QStringLiteral("city")] = entry.city.value_or(QString());
    map[QStringLiteral("region")] = entry.region.value_or(QString());
    map[QStringLiteral("postalCode")] = entry.postalCode.value_or(QString());
    map[QStringLiteral("country")] = entry.country.value_or(QString());
    return map;
}

inline QVariantMap imEntryToMap(const ContactImEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("service")] = entry.service.value_or(QString());
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("value")] = entry.value;
    return map;
}

inline QVariantMap urlEntryToMap(const ContactUrlEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("value")] = entry.value;
    return map;
}

inline QVariantMap relationEntryToMap(const ContactRelationEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("name")] = entry.name;
    return map;
}

inline QVariantMap eventEntryToMap(const ContactEventEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("date")] = entry.date;
    return map;
}

inline QVariantMap customFieldEntryToMap(const ContactCustomFieldEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label;
    map[QStringLiteral("value")] = entry.value;
    return map;
}

template <typename T, typename ToMapFn>
QVariantList entriesToVariantList(const QVector<T>& entries, ToMapFn toMap)
{
    QVariantList list;
    list.reserve(entries.size());
    for (const T& entry : entries)
        list.append(toMap(entry));
    return list;
}
