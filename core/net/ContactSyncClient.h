#pragma once

#include "models/Contact.h"
#include "net/NetworkError.h"

#include <QString>
#include <QUrl>
#include <QVector>
#include <optional>

class HttpClient;
struct RelayAuth;

// Response shape shared by both GET (pull) and POST (push)
// {serverBaseUrl}/api/contacts/sync, verified against the Go backend's
// handleContactsSync and contacts.Store.ChangedSince (see Task 16 brief).
// deletedContacts holds full Contact objects, not bare uids — the backend's
// "deleted" array carries the same Contact JSON shape as "changed", just
// with every other field zeroed/tombstoned server-side and Deleted: true
// (which is not itself a Contact field; deletedContacts vs. changed is the
// deleted flag). When tooOld is true, the server omits "changed"/"deleted"
// entirely, so both vectors are simply empty in that case — this client
// only surfaces tooOld; the reset-and-wipe behavior it implies is Phase 4
// (ContactSyncRepository), not implemented here.
struct ContactSyncResult
{
    std::optional<NetworkError> error;
    QString detail; // human-readable detail on error; empty otherwise
    qint64 cursor = 0;
    bool tooOld = false;
    QVector<Contact> changed;
    QVector<Contact> deletedContacts;
};

// Syncs contacts with the Relay backend via GET/POST {serverBaseUrl}/api/
// contacts/sync, verified against internal/api/contacts_handlers.go's
// handleContactsSync and internal/contacts/contacts.go's Contact/
// ContactValue/ContactAddress structs (see Task 16 brief) — sub/hash are
// query params on both verbs (unlike NativeRegistrationClient/
// MfaResponseClient in this batch, which take no query-param auth), and
// every core/models/Contact.h field maps 1:1 onto a same-named JSON key.
class ContactSyncClient
{
public:
    explicit ContactSyncClient(HttpClient& httpClient);

    // since = 0 requests a full initial sync.
    ContactSyncResult pull(const QUrl& serverBaseUrl, const RelayAuth& auth, qint64 since) const;

    // An empty Contact::uid within changes marks a create (server assigns
    // the real uid) -- falls out naturally since uid is a plain QString, no
    // special sentinel needed.
    ContactSyncResult push(const QUrl& serverBaseUrl, const RelayAuth& auth, qint64 baseCursor,
                            const QVector<Contact>& changes) const;

private:
    HttpClient& m_httpClient;
};
