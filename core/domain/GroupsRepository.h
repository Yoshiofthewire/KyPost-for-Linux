#pragma once

#include <QVector>

class GroupsClient;
class GroupDao;
class PairingStore;
struct Group;

// Refreshes GroupDao's local name-cache from GroupsClient, once per contact
// sync cycle. Deliberately a separate class from ContactSyncRepository
// (rather than adding GroupsClient/GroupDao as two more constructor
// dependencies there) so ContactSyncRepository -- and its large existing
// test suite, none of which expects a second outbound HTTP call per sync()
// -- stays untouched by this feature; see task-2-report.md for the
// tradeoff. ContactsController::sync() calls refresh() alongside (after a
// successful) ContactSyncRepository::sync(), duplicating the small
// pairing -> RelayAuth/serverUrl computation ContactSyncRepository::sync()
// already does internally, rather than threading it through.
//
// Small full-replace cache, no delta cursor -- matches the "small list,
// full replace" simplicity task-2-brief.md calls for (same call the source
// doc makes for Android's own GroupEntity cache).
class GroupsRepository
{
public:
    GroupsRepository(GroupsClient& client, GroupDao& groupDao, PairingStore& pairingStore);

    QVector<Group> groups() const; // groupDao.findAll()

    // No-op when not paired. On any fetch error (401, transport failure,
    // decode failure, ...) this silently gives up and leaves the existing
    // cache untouched -- never crashes, never propagates the failure to the
    // caller -- matching this feature's "GroupsClient must degrade
    // gracefully on 401/error" Global Constraint. The next sync cycle
    // simply retries; no delta cursor means a retry is always a full,
    // correct refresh.
    void refresh();

private:
    GroupsClient& m_client;
    GroupDao& m_groupDao;
    PairingStore& m_pairingStore;
};
