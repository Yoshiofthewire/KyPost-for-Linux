#include "net/ContactSyncClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {

// Appends "api/contacts/sync" to serverBaseUrl's path, mirroring
// MfaResponseClient's endpointFor -- preserves any existing path on
// serverBaseUrl and ensures exactly one slash between the two, regardless of
// whether the caller's base URL was given with or without a trailing slash.
QUrl endpointFor(const QUrl& serverBaseUrl)
{
    QUrl url = serverBaseUrl;
    QString path = url.path();
    if (!path.endsWith(QLatin1Char('/')))
        path += QLatin1Char('/');
    path += QStringLiteral("api/contacts/sync");
    url.setPath(path);
    return url;
}

// JSON mapping helpers -- kept here rather than in core/models/Contact.h so
// the plain model header stays free of wire-format concerns, matching how
// core/db/ContactDao.cpp already keeps SQL-mapping concerns out of the
// model too.

void putOptional(QJsonObject& obj, const QString& key, const std::optional<QString>& value)
{
    if (value)
        obj[key] = *value;
}

std::optional<QString> takeOptional(const QJsonObject& obj, const QString& key)
{
    if (!obj.contains(key) || obj.value(key).isNull())
        return std::nullopt;
    return obj.value(key).toString();
}

QJsonObject emailEntryToJson(const ContactEmailEntry& entry)
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

QJsonObject phoneEntryToJson(const ContactPhoneEntry& entry)
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

QJsonObject addressEntryToJson(const ContactAddressEntry& entry)
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

template <typename T, typename ToJsonFn>
QJsonArray entriesToJson(const QVector<T>& entries, ToJsonFn toJson)
{
    QJsonArray array;
    for (const T& entry : entries)
        array.append(toJson(entry));
    return array;
}

template <typename T, typename FromJsonFn>
QVector<T> entriesFromJson(const QJsonArray& array, FromJsonFn fromJson)
{
    QVector<T> entries;
    entries.reserve(array.size());
    for (const QJsonValue& value : array)
        entries.append(fromJson(value.toObject()));
    return entries;
}

} // namespace

namespace ContactWire {

// Every core/models/Contact.h field maps 1:1 onto a same-named JSON key
// (confirmed against the Go Contact/ContactValue/ContactAddress structs) --
// no field-name translation needed anywhere in this pair.
QJsonObject contactToJson(const Contact& contact)
{
    QJsonObject obj;
    obj[QStringLiteral("uid")] = contact.uid;
    obj[QStringLiteral("rev")] = contact.rev;
    putOptional(obj, QStringLiteral("createdAt"), contact.createdAt);
    putOptional(obj, QStringLiteral("updatedAt"), contact.updatedAt);
    putOptional(obj, QStringLiteral("fn"), contact.fn);
    putOptional(obj, QStringLiteral("givenName"), contact.givenName);
    putOptional(obj, QStringLiteral("familyName"), contact.familyName);
    putOptional(obj, QStringLiteral("middleName"), contact.middleName);
    putOptional(obj, QStringLiteral("prefix"), contact.prefix);
    putOptional(obj, QStringLiteral("suffix"), contact.suffix);
    putOptional(obj, QStringLiteral("nickname"), contact.nickname);
    putOptional(obj, QStringLiteral("org"), contact.org);
    putOptional(obj, QStringLiteral("title"), contact.title);
    putOptional(obj, QStringLiteral("notes"), contact.notes);
    putOptional(obj, QStringLiteral("birthday"), contact.birthday);
    obj[QStringLiteral("emails")] = entriesToJson(contact.emails, emailEntryToJson);
    obj[QStringLiteral("phones")] = entriesToJson(contact.phones, phoneEntryToJson);
    obj[QStringLiteral("addresses")] = entriesToJson(contact.addresses, addressEntryToJson);
    if (contact.deleted)
        obj[QStringLiteral("deleted")] = true;
    return obj;
}

Contact contactFromJson(const QJsonObject& obj)
{
    Contact contact;
    contact.uid = obj.value(QStringLiteral("uid")).toString();
    contact.rev = static_cast<qint64>(obj.value(QStringLiteral("rev")).toDouble());
    contact.createdAt = takeOptional(obj, QStringLiteral("createdAt"));
    contact.updatedAt = takeOptional(obj, QStringLiteral("updatedAt"));
    contact.fn = takeOptional(obj, QStringLiteral("fn"));
    contact.givenName = takeOptional(obj, QStringLiteral("givenName"));
    contact.familyName = takeOptional(obj, QStringLiteral("familyName"));
    contact.middleName = takeOptional(obj, QStringLiteral("middleName"));
    contact.prefix = takeOptional(obj, QStringLiteral("prefix"));
    contact.suffix = takeOptional(obj, QStringLiteral("suffix"));
    contact.nickname = takeOptional(obj, QStringLiteral("nickname"));
    contact.org = takeOptional(obj, QStringLiteral("org"));
    contact.title = takeOptional(obj, QStringLiteral("title"));
    contact.notes = takeOptional(obj, QStringLiteral("notes"));
    contact.birthday = takeOptional(obj, QStringLiteral("birthday"));
    contact.emails = entriesFromJson<ContactEmailEntry>(obj.value(QStringLiteral("emails")).toArray(), emailEntryFromJson);
    contact.phones = entriesFromJson<ContactPhoneEntry>(obj.value(QStringLiteral("phones")).toArray(), phoneEntryFromJson);
    contact.addresses =
        entriesFromJson<ContactAddressEntry>(obj.value(QStringLiteral("addresses")).toArray(), addressEntryFromJson);
    contact.deleted = obj.value(QStringLiteral("deleted")).toBool();
    return contact;
}

} // namespace ContactWire

namespace {

// Parses the {cursor, tooOld, changed?, deleted?} response shape shared by
// both pull and push. changed/deleted are treated as empty (not a parse
// error) when absent -- the server omits both keys entirely when tooOld is
// true (writeContactsSyncResponse only sets them if !tooOld).
ContactSyncResult parseSyncResponse(const QJsonObject& json)
{
    ContactSyncResult out;
    out.cursor = static_cast<qint64>(json.value(QStringLiteral("cursor")).toDouble());
    out.tooOld = json.value(QStringLiteral("tooOld")).toBool();

    const QJsonArray changed = json.value(QStringLiteral("changed")).toArray();
    out.changed.reserve(changed.size());
    for (const QJsonValue& value : changed)
        out.changed.append(ContactWire::contactFromJson(value.toObject()));

    const QJsonArray deleted = json.value(QStringLiteral("deleted")).toArray();
    out.deletedContacts.reserve(deleted.size());
    for (const QJsonValue& value : deleted)
        out.deletedContacts.append(ContactWire::contactFromJson(value.toObject()));

    return out;
}

} // namespace

ContactSyncClient::ContactSyncClient(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

ContactSyncResult ContactSyncClient::pull(const QUrl& serverBaseUrl, const RelayAuth& auth, qint64 since) const
{
    QList<QPair<QString, QString>> query = auth.queryItems();
    query.append({ QStringLiteral("since"), QString::number(since) });

    const HttpClient::HttpResult result = m_httpClient.get(endpointFor(serverBaseUrl), query);

    ContactSyncResult out;

    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Contact sync pull failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode contact sync response: %1").arg(parseError.errorString());
        return out;
    }

    return parseSyncResponse(doc.object());
}

ContactSyncResult ContactSyncClient::push(const QUrl& serverBaseUrl, const RelayAuth& auth, qint64 baseCursor,
                                           const QVector<Contact>& changes) const
{
    QJsonObject body;
    body[QStringLiteral("baseCursor")] = baseCursor;
    body[QStringLiteral("changes")] = entriesToJson(changes, ContactWire::contactToJson);

    const HttpClient::HttpResult result = m_httpClient.post(endpointFor(serverBaseUrl), auth.queryItems(), body);

    ContactSyncResult out;

    if (result.error.has_value()) {
        out.error = result.error;
        out.detail = !result.detail.isEmpty()
            ? result.detail
            : QStringLiteral("Contact sync push failed with status %1").arg(result.statusCode);
        return out;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.error = NetworkError::Decoding;
        out.detail = QStringLiteral("Failed to decode contact sync response: %1").arg(parseError.errorString());
        return out;
    }

    return parseSyncResponse(doc.object());
}
