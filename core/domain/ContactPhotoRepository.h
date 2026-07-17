#pragma once

#include <QString>

class ContactPhotoClient;
class ContactPhotoCache;
class PairingStore;

// Lazy per-contact photo fetch+cache, mirroring GroupsRepository's shape
// (see its doc comment for the "why a separate class" reasoning) but scoped
// to one contact at a time rather than a full-replace list. Called from
// ContactsController::photoPathFor() on contact-detail open / list-row
// become-visible (task-3-brief.md: "fetch lazily... not as part of the bulk
// sync payload") -- never from ContactSyncRepository::sync() itself.
class ContactPhotoRepository
{
public:
    ContactPhotoRepository(ContactPhotoClient& client, ContactPhotoCache& cache, PairingStore& pairingStore);

    // Returns the cached (or freshly fetched-then-cached) absolute file path
    // for contactUid's photoRef. Returns an empty string when photoRef is
    // empty, when this device isn't paired, or when the fetch fails for any
    // reason (401, transport failure, ...) -- degrades gracefully, never
    // crashes, matching this feature's GroupsClient/ContactPhotoClient
    // Global Constraint.
    QString photoPathFor(const QString& contactUid, const QString& photoRef) const;

private:
    ContactPhotoClient& m_client;
    ContactPhotoCache& m_cache;
    PairingStore& m_pairingStore;
};
