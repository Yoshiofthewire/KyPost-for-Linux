#pragma once

#include "models/Contact.h"

#include <QJsonObject>
#include <optional>

// QJsonObject encoders/decoders for Contact's struct-entry fields, shared
// between core/db/ContactDao.cpp (SQLite JSON blob columns) and
// core/net/ContactSyncClient.cpp (the wire ContactWire:: mapping) so both
// stop maintaining their own copies of the same per-field JSON shape.

inline void putOptional(QJsonObject& obj, const QString& key, const std::optional<QString>& value)
{
    if (value)
        obj[key] = *value;
}

// Missing key and explicit JSON null are both treated as "absent" -- the
// wire can send either; ContactDao only ever decodes JSON it previously
// encoded itself, which never contains an explicit null, so this is a
// no-op widening for that caller.
inline std::optional<QString> takeOptional(const QJsonObject& obj, const QString& key)
{
    if (!obj.contains(key) || obj.value(key).isNull())
        return std::nullopt;
    return obj.value(key).toString();
}

inline QJsonObject emailEntryToJson(const ContactEmailEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

inline ContactEmailEntry emailEntryFromJson(const QJsonObject& obj)
{
    ContactEmailEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

inline QJsonObject phoneEntryToJson(const ContactPhoneEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

inline ContactPhoneEntry phoneEntryFromJson(const QJsonObject& obj)
{
    ContactPhoneEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

inline QJsonObject addressEntryToJson(const ContactAddressEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    putOptional(obj, QStringLiteral("street"), entry.street);
    putOptional(obj, QStringLiteral("city"), entry.city);
    putOptional(obj, QStringLiteral("region"), entry.region);
    putOptional(obj, QStringLiteral("postalCode"), entry.postalCode);
    putOptional(obj, QStringLiteral("country"), entry.country);
    return obj;
}

inline ContactAddressEntry addressEntryFromJson(const QJsonObject& obj)
{
    ContactAddressEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.street = takeOptional(obj, QStringLiteral("street"));
    entry.city = takeOptional(obj, QStringLiteral("city"));
    entry.region = takeOptional(obj, QStringLiteral("region"));
    entry.postalCode = takeOptional(obj, QStringLiteral("postalCode"));
    entry.country = takeOptional(obj, QStringLiteral("country"));
    return entry;
}

inline QJsonObject imEntryToJson(const ContactImEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("service"), entry.service);
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

inline ContactImEntry imEntryFromJson(const QJsonObject& obj)
{
    ContactImEntry entry;
    entry.service = takeOptional(obj, QStringLiteral("service"));
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

inline QJsonObject urlEntryToJson(const ContactUrlEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

inline ContactUrlEntry urlEntryFromJson(const QJsonObject& obj)
{
    ContactUrlEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

inline QJsonObject relationEntryToJson(const ContactRelationEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("name")] = entry.name;
    return obj;
}

inline ContactRelationEntry relationEntryFromJson(const QJsonObject& obj)
{
    ContactRelationEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.name = obj.value(QStringLiteral("name")).toString();
    return entry;
}

inline QJsonObject eventEntryToJson(const ContactEventEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("date")] = entry.date;
    return obj;
}

inline ContactEventEntry eventEntryFromJson(const QJsonObject& obj)
{
    ContactEventEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.date = obj.value(QStringLiteral("date")).toString();
    return entry;
}

inline QJsonObject customFieldEntryToJson(const ContactCustomFieldEntry& entry)
{
    QJsonObject obj;
    obj[QStringLiteral("label")] = entry.label;
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

inline ContactCustomFieldEntry customFieldEntryFromJson(const QJsonObject& obj)
{
    ContactCustomFieldEntry entry;
    entry.label = obj.value(QStringLiteral("label")).toString();
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}
