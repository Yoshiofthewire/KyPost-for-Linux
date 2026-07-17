#pragma once

#include "models/Group.h"
#include "net/NetworkError.h"

#include <QString>
#include <QUrl>
#include <QVector>
#include <optional>

class HttpClient;
struct RelayAuth;

// Response from GET {serverBaseUrl}/api/groups. Must degrade gracefully on
// 401/any HTTP error -- empty groups, error set, never a crash -- per this
// feature's Global Constraints (task-2-brief.md): the backend endpoint this
// depends on is an unverified external dependency in a separate repo, never
// assume it's deployed.
struct GroupsFetchResult
{
    std::optional<NetworkError> error;
    QString detail; // human-readable detail on error; empty otherwise
    QVector<Group> groups;
};

// This repo's first per-resource GET client -- fetches the full, small list
// of backend contact groups (id/name/rev) for GroupDao's local name-cache,
// modeled directly on ContactSyncClient::pull()'s request-building/
// response-parsing shape as the closest existing precedent (see
// ContactSyncClient.cpp), even though it's a different HTTP verb/shape: a
// plain GET returning a bare JSON array, no delta cursor (task-2-brief.md:
// "no delta cursor needed, full replace" -- same call the source doc makes
// for Android).
class GroupsClient
{
public:
    explicit GroupsClient(HttpClient& httpClient);

    GroupsFetchResult fetch(const QUrl& serverBaseUrl, const RelayAuth& auth) const;

private:
    HttpClient& m_httpClient;
};
