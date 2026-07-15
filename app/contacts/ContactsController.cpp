#include "contacts/ContactsController.h"

#include "domain/ContactSyncRepository.h"
#include "models/Contact.h"

#include <KLocalizedString>

#include <QVariantList>
#include <algorithm>

namespace {

// Blank string -> std::nullopt, matching Contact's std::optional<QString>
// field convention (org/notes) rather than storing an empty-but-present
// string.
std::optional<QString> toOptional(const QString& value)
{
    return value.isEmpty() ? std::nullopt : std::make_optional(value);
}

// Shared body of createContact/updateContact's email/phone handling:
// replaces index 0 of `existing` with `newValue` (or drops it if newValue
// is blank), keeping every entry from index 1 onward byte-for-byte --
// mirrors Android's "extraEmails = dto.emails.drop(1)" preserve-untouched-
// extras pattern. Called with an empty `existing` from createContact, which
// collapses to "single-entry (or empty) list" as the brief specifies.
template <typename Entry>
QVector<Entry> replacePrimaryEntry(const QVector<Entry>& existing, const QString& newValue)
{
    const QVector<Entry> tail = existing.size() > 1 ? existing.mid(1) : QVector<Entry>();
    if (newValue.isEmpty())
        return tail;
    QVector<Entry> result;
    result.append(Entry{ std::nullopt, newValue });
    result += tail;
    return result;
}

QVariantMap emailEntryToMap(const ContactEmailEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("value")] = entry.value;
    return map;
}

QVariantMap phoneEntryToMap(const ContactPhoneEntry& entry)
{
    QVariantMap map;
    map[QStringLiteral("label")] = entry.label.value_or(QString());
    map[QStringLiteral("value")] = entry.value;
    return map;
}

QVariantMap addressEntryToMap(const ContactAddressEntry& entry)
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

std::optional<Contact> findByUid(const QVector<Contact>& contacts, const QString& uid)
{
    const auto it = std::find_if(contacts.begin(), contacts.end(),
                                  [&uid](const Contact& c) { return c.uid == uid; });
    if (it == contacts.end())
        return std::nullopt;
    return *it;
}

} // namespace

ContactsController::ContactsController(ContactSyncRepository& repository, QObject* parent)
    : QObject(parent)
    , m_repository(repository)
    , m_model(new ContactListModel(this))
{
    // Deliberately does NOT call load() here -- matches MailController's
    // existing convention (its model starts empty until QML calls
    // selectFolder()) rather than introducing a second, inconsistent
    // eager-populate-on-construction pattern. QML is expected to call
    // load() itself (e.g. from Component.onCompleted), same as it already
    // does for MailApp.
}

QObject* ContactsController::contactModel() const
{
    return m_model;
}

bool ContactsController::isBusy() const
{
    return m_isBusy;
}

QString ContactsController::lastError() const
{
    return m_lastError;
}

QString ContactsController::statusMessage() const
{
    return m_statusMessage;
}

void ContactsController::setBusy(bool busy)
{
    if (m_isBusy == busy)
        return;
    m_isBusy = busy;
    emit isBusyChanged();
}

void ContactsController::setLastError(const QString& error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void ContactsController::setStatusMessage(const QString& message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void ContactsController::load()
{
    m_model->setContacts(m_repository.contacts());
}

void ContactsController::sync()
{
    setBusy(true);
    const ContactSyncOutcome outcome = m_repository.sync();
    setBusy(false);

    // Mirrors Android's ContactSyncOutcome toast mapping in
    // ContactsListActivity: one short user-facing string per
    // ContactSyncStatus value.
    switch (outcome.status) {
    case ContactSyncStatus::Success:
        setLastError(QString());
        setStatusMessage(i18n("Synced -- %1 pushed, %2 applied", outcome.summary.pushed, outcome.summary.applied));
        load();
        break;
    case ContactSyncStatus::NotPaired:
        setStatusMessage(QString());
        setLastError(i18n("Not paired"));
        break;
    case ContactSyncStatus::Unauthorized:
        setStatusMessage(QString());
        setLastError(i18n("Unauthorized -- please re-pair this device"));
        break;
    case ContactSyncStatus::ServiceUnavailable:
        setStatusMessage(QString());
        setLastError(outcome.detail.isEmpty() ? i18n("Service unavailable") : outcome.detail);
        break;
    case ContactSyncStatus::Retry:
        setStatusMessage(QString());
        setLastError(outcome.detail.isEmpty() ? i18n("Sync failed, try again") : outcome.detail);
        break;
    }
}

QVariantMap ContactsController::contactAt(const QString& uid)
{
    const std::optional<Contact> found = findByUid(m_repository.contacts(), uid);
    if (!found)
        return {};
    const Contact& c = *found;

    QVariantMap map;
    map[QStringLiteral("uid")] = c.uid;
    map[QStringLiteral("rev")] = c.rev;
    map[QStringLiteral("createdAt")] = c.createdAt.value_or(QString());
    map[QStringLiteral("updatedAt")] = c.updatedAt.value_or(QString());
    map[QStringLiteral("fn")] = c.fn.value_or(QString());
    map[QStringLiteral("givenName")] = c.givenName.value_or(QString());
    map[QStringLiteral("familyName")] = c.familyName.value_or(QString());
    map[QStringLiteral("middleName")] = c.middleName.value_or(QString());
    map[QStringLiteral("prefix")] = c.prefix.value_or(QString());
    map[QStringLiteral("suffix")] = c.suffix.value_or(QString());
    map[QStringLiteral("nickname")] = c.nickname.value_or(QString());
    map[QStringLiteral("org")] = c.org.value_or(QString());
    map[QStringLiteral("title")] = c.title.value_or(QString());
    map[QStringLiteral("notes")] = c.notes.value_or(QString());
    map[QStringLiteral("birthday")] = c.birthday.value_or(QString());

    QVariantList emails;
    emails.reserve(c.emails.size());
    for (const ContactEmailEntry& entry : c.emails)
        emails.append(emailEntryToMap(entry));
    map[QStringLiteral("emails")] = emails;

    QVariantList phones;
    phones.reserve(c.phones.size());
    for (const ContactPhoneEntry& entry : c.phones)
        phones.append(phoneEntryToMap(entry));
    map[QStringLiteral("phones")] = phones;

    QVariantList addresses;
    addresses.reserve(c.addresses.size());
    for (const ContactAddressEntry& entry : c.addresses)
        addresses.append(addressEntryToMap(entry));
    map[QStringLiteral("addresses")] = addresses;

    map[QStringLiteral("deleted")] = c.deleted;
    return map;
}

QString ContactsController::createContact(const QVariantMap& fields)
{
    const QString fn = fields.value(QStringLiteral("fn")).toString().trimmed();
    if (fn.isEmpty()) {
        setLastError(i18n("Name is required"));
        return QString();
    }

    Contact contact;
    contact.fn = fn;
    contact.org = toOptional(fields.value(QStringLiteral("org")).toString());
    contact.notes = toOptional(fields.value(QStringLiteral("notes")).toString());
    contact.emails =
        replacePrimaryEntry<ContactEmailEntry>({}, fields.value(QStringLiteral("email")).toString().trimmed());
    contact.phones =
        replacePrimaryEntry<ContactPhoneEntry>({}, fields.value(QStringLiteral("phone")).toString().trimmed());

    const QString newUid = m_repository.queueCreate(contact);
    setLastError(QString());
    load();
    return newUid;
}

bool ContactsController::updateContact(const QString& uid, const QVariantMap& fields)
{
    const QString fn = fields.value(QStringLiteral("fn")).toString().trimmed();
    if (fn.isEmpty()) {
        setLastError(i18n("Name is required"));
        return false;
    }

    const std::optional<Contact> found = findByUid(m_repository.contacts(), uid);
    if (!found) {
        setLastError(i18n("Contact not found"));
        return false;
    }

    Contact contact = *found;
    contact.fn = fn;
    contact.org = toOptional(fields.value(QStringLiteral("org")).toString());
    contact.notes = toOptional(fields.value(QStringLiteral("notes")).toString());
    contact.emails = replacePrimaryEntry<ContactEmailEntry>(
        contact.emails, fields.value(QStringLiteral("email")).toString().trimmed());
    contact.phones = replacePrimaryEntry<ContactPhoneEntry>(
        contact.phones, fields.value(QStringLiteral("phone")).toString().trimmed());

    m_repository.queueUpdate(contact);
    setLastError(QString());
    load();
    return true;
}

bool ContactsController::deleteContact(const QString& uid, qint64 rev)
{
    m_repository.queueDelete(uid, rev);
    setLastError(QString());
    load();
    return true;
}
