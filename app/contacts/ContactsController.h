#pragma once

#include "contacts/ContactListModel.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

class ContactSyncRepository;

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
class ContactsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* contactModel READ contactModel CONSTANT)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged) // last sync outcome, human-readable, "" when none

public:
    explicit ContactsController(ContactSyncRepository& repository, QObject* parent = nullptr);

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

signals:
    void isBusyChanged();
    void lastErrorChanged();
    void statusMessageChanged();

private:
    void setBusy(bool busy);
    void setLastError(const QString& error);
    void setStatusMessage(const QString& message);

    ContactSyncRepository& m_repository;
    ContactListModel* m_model; // owned, parented to this
    bool m_isBusy = false;
    QString m_lastError;
    QString m_statusMessage;
};
