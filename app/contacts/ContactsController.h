#pragma once

#include "contacts/ContactListModel.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

class ContactSyncRepository;
class GroupsRepository;
class ContactPhotoRepository;

// QML-facing bridge (Task 33) over core/domain's ContactSyncRepository.
// Registered as the "ContactsApp" QML singleton in main.cpp. Every method
// here that reaches the network (sync()) runs synchronously on the calling
// (GUI) thread -- see Phase 6 global constraint 2, this is a known, accepted
// freeze-the-UI tradeoff for this phase, not a bug.
//
// createContact/updateContact's QVariantMap fields contract (matches Task
// 36's edit form field set, itself matching Mac's simpler 3-field edit form
// over Android's richer one): keys fn (QString, required -- rejected with
// lastError set if blank/whitespace-only, matching both reference clients'
// "name is the only required field" rule), org (QString, optional), notes
// (QString, optional), email (QString, optional -- becomes emails:
// [{value: email}] if non-blank, else an empty list), phone (QString, same
// rule as email). On updateContact, entries beyond index 0 of the existing
// contact's emails/phones are preserved byte-for-byte (mirrors Android's
// "extraEmails = dto.emails.drop(1)" preserve-untouched-extras pattern) --
// only index 0 is replaced (or dropped, if the field is blank and there are
// no further entries) by the form's single email/phone field. createContact
// has no existing contact to preserve extras from, so it just builds a
// single-entry (or empty) list per field.
//
// Extended-contact-fields keys (Task 1 of that feature -- no edit-form UI
// consumes these yet, but the QVariantMap <-> Contact mapping exists so a
// later UI task can): groupIds (QVariantList<QString>), photoRef (QString,
// optional), pgpKey (QString, optional), ims (QVariantList<QVariantMap
// {service, label, value}>), websites (QVariantList<QVariantMap {label,
// value}>), relations (QVariantList<QVariantMap {label, name}>), events
// (QVariantList<QVariantMap {label, date}>), phoneticGivenName (QString,
// optional), phoneticFamilyName (QString, optional), department (QString,
// optional), customFields (QVariantList<QVariantMap {label, value}>),
// pronouns (QString, optional). Unlike email/phone, every one of these is a
// whole-value/whole-list replace on both createContact and updateContact --
// there's no existing single-value UI precedent to preserve extras around,
// per the field-by-field mapping table in task-1-brief.md.
class ContactsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* contactModel READ contactModel CONSTANT)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged) // last sync outcome, human-readable, "" when none

public:
    // groupsRepository: Task 2's "refresh the groups name-cache once per
    // contact sync cycle" step -- sync() calls groupsRepository.refresh()
    // once after a successful contact sync pass (see sync()'s .cpp comment
    // for why only on Success), matching this repo's existing
    // mandatory-reference constructor-injection convention (no optional/
    // nullable dependencies elsewhere in app/contacts/ or core/domain/).
    // photoRepository: extended-contact-fields Task 3's lazy per-contact
    // photo fetch+cache (core/domain/ContactPhotoRepository.h) -- called
    // only from photoPathFor() below, on demand, never from sync() (photos
    // are deliberately NOT part of the bulk contact sync payload, per
    // task-3-brief.md).
    ContactsController(ContactSyncRepository& repository, GroupsRepository& groupsRepository,
                        ContactPhotoRepository& photoRepository, QObject* parent = nullptr);

    QObject* contactModel() const;
    bool isBusy() const;
    QString lastError() const;
    QString statusMessage() const;

public slots:
    void load(); // refreshes the model from repository.contacts(), no network call
    void sync(); // calls repository.sync(), maps ContactSyncOutcome.status to statusMessage/lastError, reloads model on Success
    QVariantMap contactAt(const QString& uid); // full Contact struct as a QVariantMap (all fields incl. nested emails/phones/addresses as QVariantList of QVariantMap) for the edit form -- returns {} if not found
    QString createContact(const QVariantMap& fields); // builds a Contact from fields, calls repository.queueCreate(), returns the new local uid ("" on rejection)
    bool updateContact(const QString& uid, const QVariantMap& fields); // loads existing Contact via repository.contacts()/find-by-uid, applies fields, calls repository.queueUpdate(); returns false if uid not found or fn blank
    bool deleteContact(const QString& uid, qint64 rev); // calls repository.queueDelete(uid, rev)

    // extended-contact-fields Task 3: lazy per-contact photo fetch+cache
    // entry point for QML -- Avatar.qml's photoSource binds to this call
    // (ContactsList.qml's row delegate / ContactDetail.qml's read-only
    // card), not to any property, so it re-runs per row/detail-open the
    // same way contactAt() already does. Looks up uid's Contact via
    // repository.contacts(), and if it has a non-empty photoRef, delegates
    // to photoRepository.photoPathFor() (cache hit or synchronous
    // fetch-then-cache -- see that class's doc comment for the same
    // synchronous-on-GUI-thread tradeoff sync() above already carries).
    // Returns a file:// URL string ready for Image.source, or "" if there's
    // no photoRef, no pairing, or the fetch failed -- Avatar.qml's fallback
    // to initials handles every one of those "" cases identically, so this
    // never needs to distinguish them for QML.
    QString photoPathFor(const QString& uid);

signals:
    void isBusyChanged();
    void lastErrorChanged();
    void statusMessageChanged();

private:
    void setBusy(bool busy);
    void setLastError(const QString& error);
    void setStatusMessage(const QString& message);

    ContactSyncRepository& m_repository;
    GroupsRepository& m_groupsRepository;
    ContactPhotoRepository& m_photoRepository;
    ContactListModel* m_model; // owned, parented to this
    bool m_isBusy = false;
    QString m_lastError;
    QString m_statusMessage;
};
