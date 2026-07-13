#include "domain/DeviceRegistrationService.h"

#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "stores/SettingsStore.h"

DeviceRegistrationService::DeviceRegistrationService(NativeRegistrationClient& client, PairingStore& pairingStore,
                                                       SettingsStore& settingsStore)
    : m_client(client)
    , m_pairingStore(pairingStore)
    , m_settingsStore(settingsStore)
{
}

NativeRegistrationResult DeviceRegistrationService::pair(const PairingParams& params, const QString& deviceToken)
{
    const NativeRegistrationResult result =
        m_client.registerDevice(QUrl(params.registrationUrl), params.subscriberId,
                                 params.subscriberHash.isEmpty() ? std::nullopt
                                                                  : std::make_optional(params.subscriberHash),
                                 params.pairingToken, deviceToken, QString(), params.deviceName);

    if (result.outcome != RegistrationOutcome::Success)
        return result;

    DevicePairing pairing;
    pairing.subscriberId = params.subscriberId;
    pairing.subscriberHash = params.subscriberHash;
    pairing.serverBaseUrl = params.serverBaseUrl;
    pairing.registrationUrl = params.registrationUrl;
    pairing.pairingToken = params.pairingToken;
    pairing.deviceId = result.response.deviceId;
    pairing.deviceName = params.deviceName;
    m_pairingStore.save(pairing);

    m_settingsStore.setDeliveryMode(result.response.deliveryMode);
    m_settingsStore.setTransport(result.response.transport);
    m_settingsStore.setPullEndpoint(result.response.pullEndpoint);

    return result;
}

std::optional<NativeRegistrationResult> DeviceRegistrationService::reregisterIfPaired(const QString& deviceToken)
{
    const std::optional<DevicePairing> pairing = m_pairingStore.load();
    if (!pairing.has_value())
        return std::nullopt;

    PairingParams params;
    params.subscriberId = pairing->subscriberId;
    params.subscriberHash = pairing->subscriberHash;
    params.serverBaseUrl = pairing->serverBaseUrl;
    params.registrationUrl = pairing->registrationUrl;
    params.pairingToken = pairing->pairingToken;
    params.deviceName = pairing->deviceName;

    return pair(params, deviceToken);
}
