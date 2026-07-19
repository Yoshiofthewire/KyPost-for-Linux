#include "domain/ContactPhotoRepository.h"

#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "net/ContactPhotoClient.h"
#include "net/RelayAuth.h"
#include "stores/ContactPhotoCache.h"

#include <QUrl>

ContactPhotoRepository::ContactPhotoRepository(ContactPhotoClient& client, ContactPhotoCache& cache,
                                                 PairingStore& pairingStore)
    : m_client(client)
    , m_cache(cache)
    , m_pairingStore(pairingStore)
{
}

QString ContactPhotoRepository::photoPathFor(const QString& contactUid, const QString& photoRef) const
{
    if (photoRef.isEmpty())
        return QString();

    const QString cached = m_cache.cachedPathFor(photoRef);
    if (!cached.isEmpty())
        return cached;

    const std::optional<DevicePairing> pairing = m_pairingStore.load();
    if (!pairing.has_value())
        return QString(); // not paired -- degrade gracefully, no crash

    const RelayAuth auth{ pairing->deviceId, pairing->deviceSecret };
    const QUrl serverUrl(pairing->serverBaseUrl);

    const ContactPhotoFetchResult result = m_client.fetch(serverUrl, contactUid, auth);
    if (result.error.has_value() || result.photoBytes.isEmpty())
        return QString(); // degrade gracefully -- next call simply retries

    return m_cache.store(photoRef, result.photoBytes);
}
