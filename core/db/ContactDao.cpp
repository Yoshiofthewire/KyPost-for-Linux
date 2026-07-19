#include "ContactDao.h"

#include "SqlUtil.h"
#include "models/ContactFieldJson.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlQuery>

namespace {

// Per-entry toJson/fromJson mapping comes from models/ContactFieldJson.h,
// shared with core/net/ContactSyncClient.cpp's wire mapping.

template <typename T, typename ToJsonFn>
QString encodeEntries(const QVector<T>& entries, ToJsonFn toJson)
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

// groupIds is a plain QVector<QString>, not a struct-entry list, so it gets
// its own encode/decode pair rather than going through toJson/decodeEntries.
QString encodeStringList(const QVector<QString>& values)
{
    QJsonArray array;
    for (const QString& value : values)
        array.append(value);
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<QString> decodeStringList(const QString& json)
{
    QVector<QString> values;
    if (json.isEmpty())
        return values;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    for (const QJsonValue& value : doc.array())
        values.append(value.toString());
    return values;
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
    contact.groupIds = decodeStringList(query.value(QStringLiteral("groups_json")).toString());
    contact.photoRef = variantToOptionalString(query.value(QStringLiteral("photo_ref")));
    contact.pgpKey = variantToOptionalString(query.value(QStringLiteral("pgp_key")));
    contact.ims = decodeEntries<ContactImEntry>(
        query.value(QStringLiteral("ims_json")).toString(), imEntryFromJson);
    contact.websites = decodeEntries<ContactUrlEntry>(
        query.value(QStringLiteral("websites_json")).toString(), urlEntryFromJson);
    contact.relations = decodeEntries<ContactRelationEntry>(
        query.value(QStringLiteral("relations_json")).toString(), relationEntryFromJson);
    contact.events = decodeEntries<ContactEventEntry>(
        query.value(QStringLiteral("events_json")).toString(), eventEntryFromJson);
    contact.phoneticGivenName = variantToOptionalString(query.value(QStringLiteral("phonetic_given_name")));
    contact.phoneticFamilyName = variantToOptionalString(query.value(QStringLiteral("phonetic_family_name")));
    contact.department = variantToOptionalString(query.value(QStringLiteral("department")));
    contact.customFields = decodeEntries<ContactCustomFieldEntry>(
        query.value(QStringLiteral("custom_fields_json")).toString(), customFieldEntryFromJson);
    contact.pronouns = variantToOptionalString(query.value(QStringLiteral("pronouns")));
    contact.isSelf = query.value(QStringLiteral("is_self")).toBool();
    contact.mergedUIDs = decodeStringList(query.value(QStringLiteral("merged_uids_json")).toString());
    contact.mergedInto = variantToOptionalString(query.value(QStringLiteral("merged_into")));
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
        "suffix, nickname, org, title, notes, birthday, emails_json, phones_json, addresses_json, "
        "groups_json, photo_ref, pgp_key, ims_json, websites_json, relations_json, events_json, "
        "phonetic_given_name, phonetic_family_name, department, custom_fields_json, pronouns, "
        "is_self, merged_uids_json, merged_into) "
        "VALUES (:uid, :rev, :created_at, :updated_at, :fn, :given_name, :family_name, "
        ":middle_name, :prefix, :suffix, :nickname, :org, :title, :notes, :birthday, "
        ":emails_json, :phones_json, :addresses_json, "
        ":groups_json, :photo_ref, :pgp_key, :ims_json, :websites_json, :relations_json, :events_json, "
        ":phonetic_given_name, :phonetic_family_name, :department, :custom_fields_json, :pronouns, "
        ":is_self, :merged_uids_json, :merged_into)"));
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
    query.bindValue(QStringLiteral(":emails_json"), encodeEntries(contact.emails, emailEntryToJson));
    query.bindValue(QStringLiteral(":phones_json"), encodeEntries(contact.phones, phoneEntryToJson));
    query.bindValue(QStringLiteral(":addresses_json"), encodeEntries(contact.addresses, addressEntryToJson));
    query.bindValue(QStringLiteral(":groups_json"), encodeStringList(contact.groupIds));
    query.bindValue(QStringLiteral(":photo_ref"), optionalStringToVariant(contact.photoRef));
    query.bindValue(QStringLiteral(":pgp_key"), optionalStringToVariant(contact.pgpKey));
    query.bindValue(QStringLiteral(":ims_json"), encodeEntries(contact.ims, imEntryToJson));
    query.bindValue(QStringLiteral(":websites_json"), encodeEntries(contact.websites, urlEntryToJson));
    query.bindValue(QStringLiteral(":relations_json"), encodeEntries(contact.relations, relationEntryToJson));
    query.bindValue(QStringLiteral(":events_json"), encodeEntries(contact.events, eventEntryToJson));
    query.bindValue(QStringLiteral(":phonetic_given_name"), optionalStringToVariant(contact.phoneticGivenName));
    query.bindValue(QStringLiteral(":phonetic_family_name"), optionalStringToVariant(contact.phoneticFamilyName));
    query.bindValue(QStringLiteral(":department"), optionalStringToVariant(contact.department));
    query.bindValue(QStringLiteral(":custom_fields_json"), encodeEntries(contact.customFields, customFieldEntryToJson));
    query.bindValue(QStringLiteral(":pronouns"), optionalStringToVariant(contact.pronouns));
    query.bindValue(QStringLiteral(":is_self"), contact.isSelf);
    query.bindValue(QStringLiteral(":merged_uids_json"), encodeStringList(contact.mergedUIDs));
    query.bindValue(QStringLiteral(":merged_into"), optionalStringToVariant(contact.mergedInto));
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
