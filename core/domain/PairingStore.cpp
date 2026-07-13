#include "domain/PairingStore.h"

#include "stores/SecureStore.h"

namespace {

constexpr auto kSubscriberIdKey = "sub";
constexpr auto kSubscriberHashKey = "hash";
constexpr auto kServerBaseUrlKey = "pairing.serverBaseUrl";
constexpr auto kRegistrationUrlKey = "pairing.registrationUrl";
constexpr auto kPairingTokenKey = "pairing.pairingToken";
constexpr auto kDeviceIdKey = "deviceId";
constexpr auto kDeviceNameKey = "pairing.deviceName";

QString valueOrEmpty(const std::optional<QString>& value)
{
    return value.value_or(QString());
}

}

PairingStore::PairingStore(SecureStore& secureStore)
    : m_secureStore(secureStore)
{
}

std::optional<DevicePairing> PairingStore::load() const
{
    const std::optional<QString> subscriberId = m_secureStore.get(QLatin1String(kSubscriberIdKey));
    if (!subscriberId.has_value())
        return std::nullopt;

    DevicePairing pairing;
    pairing.subscriberId = *subscriberId;
    pairing.subscriberHash = valueOrEmpty(m_secureStore.get(QLatin1String(kSubscriberHashKey)));
    pairing.serverBaseUrl = valueOrEmpty(m_secureStore.get(QLatin1String(kServerBaseUrlKey)));
    pairing.registrationUrl = valueOrEmpty(m_secureStore.get(QLatin1String(kRegistrationUrlKey)));
    pairing.pairingToken = valueOrEmpty(m_secureStore.get(QLatin1String(kPairingTokenKey)));
    pairing.deviceId = valueOrEmpty(m_secureStore.get(QLatin1String(kDeviceIdKey)));
    pairing.deviceName = valueOrEmpty(m_secureStore.get(QLatin1String(kDeviceNameKey)));
    return pairing;
}

bool PairingStore::save(const DevicePairing& pairing)
{
    bool ok = true;
    ok = m_secureStore.set(QLatin1String(kSubscriberIdKey), pairing.subscriberId) && ok;
    ok = m_secureStore.set(QLatin1String(kSubscriberHashKey), pairing.subscriberHash) && ok;
    ok = m_secureStore.set(QLatin1String(kServerBaseUrlKey), pairing.serverBaseUrl) && ok;
    ok = m_secureStore.set(QLatin1String(kRegistrationUrlKey), pairing.registrationUrl) && ok;
    ok = m_secureStore.set(QLatin1String(kPairingTokenKey), pairing.pairingToken) && ok;
    ok = m_secureStore.set(QLatin1String(kDeviceIdKey), pairing.deviceId) && ok;
    ok = m_secureStore.set(QLatin1String(kDeviceNameKey), pairing.deviceName) && ok;
    return ok;
}

void PairingStore::clear()
{
    m_secureStore.remove(QLatin1String(kSubscriberIdKey));
    m_secureStore.remove(QLatin1String(kSubscriberHashKey));
    m_secureStore.remove(QLatin1String(kServerBaseUrlKey));
    m_secureStore.remove(QLatin1String(kRegistrationUrlKey));
    m_secureStore.remove(QLatin1String(kPairingTokenKey));
    m_secureStore.remove(QLatin1String(kDeviceIdKey));
    m_secureStore.remove(QLatin1String(kDeviceNameKey));
}

bool PairingStore::isPaired() const
{
    return load().has_value();
}
