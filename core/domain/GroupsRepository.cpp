#include "domain/GroupsRepository.h"

#include "db/GroupDao.h"
#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "models/Group.h"
#include "net/GroupsClient.h"
#include "net/RelayAuth.h"

#include <QUrl>

GroupsRepository::GroupsRepository(GroupsClient& client, GroupDao& groupDao, PairingStore& pairingStore)
    : m_client(client)
    , m_groupDao(groupDao)
    , m_pairingStore(pairingStore)
{
}

QVector<Group> GroupsRepository::groups() const
{
    return m_groupDao.findAll();
}

void GroupsRepository::refresh()
{
    const std::optional<DevicePairing> pairing = m_pairingStore.load();
    if (!pairing.has_value())
        return;

    const RelayAuth auth{ pairing->subscriberId, pairing->subscriberHash };
    const QUrl serverUrl(pairing->serverBaseUrl);

    const GroupsFetchResult result = m_client.fetch(serverUrl, auth);
    if (result.error.has_value())
        return; // degrade gracefully -- next sync cycle retries, no crash

    for (const Group& group : result.groups)
        m_groupDao.insertOrReplace(group);
}
