#pragma once

#include "models/Contact.h"

#include <QAbstractListModel>
#include <QHash>
#include <QVariant>
#include <QVector>

// QML-facing read-only model wrapping a QVector<Contact> (Task 33), same
// pattern as EmailListModel (see app/mail/EmailListModel.h): owned by
// ContactsController, which is the only writer via setContacts(). One row
// per Contact. primaryEmail/primaryPhone are derived (first entry of
// emails/phones, empty string if none) rather than exposing the full
// nested-entry arrays as roles -- those are the two fields every list row
// and the Mac-style read-only card actually display; the full Contact
// struct (including every email/phone/address entry) is available to QML
// for the edit form via ContactsController::contactAt(uid) instead.
//
// synced role: `rev != 0`. Task 33's brief flags a real gap here --
// ContactSyncRepository::queueCreate() (see its .cpp) assigns a temp local
// uid to a freshly-created-but-not-yet-synced contact *immediately*, so
// `uid.isEmpty()` is NOT a usable "is this synced" test (every local
// contact, synced or not, has a non-empty uid). There is currently no field
// on core/models/Contact.h that distinguishes "temp local uid" from "real
// server uid" -- the "Local"/"Synced" badge distinction from both reference
// clients cannot be built correctly from Contact as it stands today.
// `rev` is the best available proxy: queueCreate() inserts the local cache
// row with whatever rev the caller's Contact carried (ContactsController::
// createContact() always builds a fresh Contact, so this is the struct's
// rev=0 default), and rev is only ever overwritten with a real value when
// ContactSyncRepository::sync() applies a server response (mergeContact()
// always takes the response's rev directly, never merged -- see sync()'s
// "for (const Contact& c : result.changed)" loop). Confirmed against
// tests/core/domain/ContactSyncRepositoryTest.cpp's fixtures: every
// server-returned contact in that suite carries "rev":1 or higher, never 0.
// So rev==0 reliably means "this row has never round-tripped through a
// successful sync" for every path that goes through queueCreate()+sync().
// Limits: this still can't distinguish "queued update not yet pushed" from
// "successfully synced" (queueUpdate() doesn't touch rev, so an already-
// synced contact with a pending edit still reads as synced=true, which is
// arguably the more useful reading for a "Local"/"Synced" badge anyway --
// the contact IS a real server object, just with an unflushed edit) and
// theoretically breaks if a server ever legitimately returns rev==0 for a
// freshly created object (not observed in this codebase's fixtures/tests).
class ContactListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        UidRole = Qt::UserRole + 1,
        RevRole,
        FnRole,
        GivenNameRole,
        FamilyNameRole,
        OrgRole,
        NotesRole,
        PrimaryEmailRole,
        PrimaryPhoneRole,
        SyncedRole,
        // extended-contact-fields Task 3: exposes Contact::photoRef to
        // ContactsList.qml's row delegate so it can call
        // ContactsApp.photoPathFor(model.uid) for its Avatar -- deliberately
        // NOT added back in Task 1, which left this model untouched since
        // nothing consumed photoRef from a list row yet (see this class's
        // own doc comment: primaryEmail/primaryPhone are the only two
        // derived-from-nested-data roles this model exposes, everything
        // else is a scalar Contact field, and photoRef is the same shape as
        // those).
        PhotoRefRole,
    };

    explicit ContactListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setContacts(const QVector<Contact>& contacts);
    Contact contactAt(int row) const; // out-of-range -> default-constructed Contact

private:
    QVector<Contact> m_contacts;
};
