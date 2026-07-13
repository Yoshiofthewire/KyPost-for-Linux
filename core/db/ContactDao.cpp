#include "ContactDao.h"

#include "SqlUtil.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlQuery>

namespace {

void putOptional(QJsonObject& obj, const QString& key, const std::optional<QString>& value)
{
    if (value)
        obj[key] = *value;
}

std::optional<QString> takeOptional(const QJsonObject& obj, const QString& key)
{
    if (!obj.contains(key))
        return std::nullopt;
    return obj.value(key).toString();
}

QJsonObject toJson(const ContactEmailEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

ContactEmailEntry emailEntryFromJson(const QJsonObject& obj)
{
    ContactEmailEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

QJsonObject toJson(const ContactPhoneEntry& entry)
{
    QJsonObject obj;
    putOptional(obj, QStringLiteral("label"), entry.label);
    obj[QStringLiteral("value")] = entry.value;
    return obj;
}

ContactPhoneEntry phoneEntryFromJson(const QJsonObject& obj)
{
    ContactPhoneEntry entry;
    entry.label = takeOptional(obj, QStringLiteral("label"));
    entry.value = obj.value(QStringLiteral("value")).toString();
    return entry;
}

QJsonObject toJson(const ContactAddressEntry& entry)
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

ContactAddressEntry addressEntryFromJson(const QJsonObject& obj)
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

template <typename T>
QString encodeEntries(const QVector<T>& entries)
{
    QJsonArray array;
    for (const T& entry : entries)
        array.append(toJson(entry));
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

template <typename T, typename FromJsonFn>
QVector<T> decodeEntries(const QString& json, FromJsonFn fromJson)
{
    QVector<T> entries;
    if (json.isEmpty())
        return entries;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    for (const QJsonValue& value : doc.array())
        entries.append(fromJson(value.toObject()));
    return entries;
}

Contact contactFromQuery(const QSqlQuery& query)
{
    Contact contact;
    contact.uid = query.value(QStringLiteral("uid")).toString();
    contact.rev = query.value(QStringLiteral("rev")).toLongLong();
    contact.createdAt = variantToOptionalString(query.value(QStringLiteral("created_at")));
    contact.updatedAt = variantToOptionalString(query.value(QStringLiteral("updated_at")));
    contact.fn = variantToOptionalString(query.value(QStringLiteral("fn")));
    contact.givenName = variantToOptionalString(query.value(QStringLiteral("given_name")));
    contact.familyName = variantToOptionalString(query.value(QStringLiteral("family_name")));
    contact.middleName = variantToOptionalString(query.value(QStringLiteral("middle_name")));
    contact.prefix = variantToOptionalString(query.value(QStringLiteral("prefix")));
    contact.suffix = variantToOptionalString(query.value(QStringLiteral("suffix")));
    contact.nickname = variantToOptionalString(query.value(QStringLiteral("nickname")));
    contact.org = variantToOptionalString(query.value(QStringLiteral("org")));
    contact.title = variantToOptionalString(query.value(QStringLiteral("title")));
    contact.notes = variantToOptionalString(query.value(QStringLiteral("notes")));
    contact.birthday = variantToOptionalString(query.value(QStringLiteral("birthday")));
    contact.emails = decodeEntries<ContactEmailEntry>(
        query.value(QStringLiteral("emails_json")).toString(), emailEntryFromJson);
    contact.phones = decodeEntries<ContactPhoneEntry>(
        query.value(QStringLiteral("phones_json")).toString(), phoneEntryFromJson);
    contact.addresses = decodeEntries<ContactAddressEntry>(
        query.value(QStringLiteral("addresses_json")).toString(), addressEntryFromJson);
    return contact;
}

} // namespace

ContactDao::ContactDao(QSqlDatabase& db) : m_db(db)
{
}

bool ContactDao::insertOrReplace(const Contact& contact)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO contacts "
        "(uid, rev, created_at, updated_at, fn, given_name, family_name, middle_name, prefix, "
        "suffix, nickname, org, title, notes, birthday, emails_json, phones_json, addresses_json) "
        "VALUES (:uid, :rev, :created_at, :updated_at, :fn, :given_name, :family_name, "
        ":middle_name, :prefix, :suffix, :nickname, :org, :title, :notes, :birthday, "
        ":emails_json, :phones_json, :addresses_json)"));
    query.bindValue(QStringLiteral(":uid"), contact.uid);
    query.bindValue(QStringLiteral(":rev"), contact.rev);
    query.bindValue(QStringLiteral(":created_at"), optionalStringToVariant(contact.createdAt));
    query.bindValue(QStringLiteral(":updated_at"), optionalStringToVariant(contact.updatedAt));
    query.bindValue(QStringLiteral(":fn"), optionalStringToVariant(contact.fn));
    query.bindValue(QStringLiteral(":given_name"), optionalStringToVariant(contact.givenName));
    query.bindValue(QStringLiteral(":family_name"), optionalStringToVariant(contact.familyName));
    query.bindValue(QStringLiteral(":middle_name"), optionalStringToVariant(contact.middleName));
    query.bindValue(QStringLiteral(":prefix"), optionalStringToVariant(contact.prefix));
    query.bindValue(QStringLiteral(":suffix"), optionalStringToVariant(contact.suffix));
    query.bindValue(QStringLiteral(":nickname"), optionalStringToVariant(contact.nickname));
    query.bindValue(QStringLiteral(":org"), optionalStringToVariant(contact.org));
    query.bindValue(QStringLiteral(":title"), optionalStringToVariant(contact.title));
    query.bindValue(QStringLiteral(":notes"), optionalStringToVariant(contact.notes));
    query.bindValue(QStringLiteral(":birthday"), optionalStringToVariant(contact.birthday));
    query.bindValue(QStringLiteral(":emails_json"), encodeEntries(contact.emails));
    query.bindValue(QStringLiteral(":phones_json"), encodeEntries(contact.phones));
    query.bindValue(QStringLiteral(":addresses_json"), encodeEntries(contact.addresses));
    return query.exec();
}

std::optional<Contact> ContactDao::findById(const QString& uid) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT * FROM contacts WHERE uid = :uid"));
    query.bindValue(QStringLiteral(":uid"), uid);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return contactFromQuery(query);
}

QVector<Contact> ContactDao::findAll() const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT * FROM contacts")))
        return {};
    QVector<Contact> results;
    while (query.next())
        results.append(contactFromQuery(query));
    return results;
}

bool ContactDao::deleteById(const QString& uid)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM contacts WHERE uid = :uid"));
    query.bindValue(QStringLiteral(":uid"), uid);
    return query.exec();
}

bool ContactDao::deleteAll()
{
    QSqlQuery query(m_db);
    return query.exec(QStringLiteral("DELETE FROM contacts"));
}
