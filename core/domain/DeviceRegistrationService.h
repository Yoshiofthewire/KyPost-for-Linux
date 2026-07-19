#pragma once

#include "net/NativeRegistrationClient.h"

#include <QString>
#include <optional>

class PairingStore;
class SettingsStore;

// Initial-pairing and re-registration params for DeviceRegistrationService::
// pair(). Mirrors DevicePairing's six persisted fields minus deviceId, which
// the server assigns on first pairing and DeviceRegistrationService supplies
// itself on re-registration (from the currently-stored DevicePairing).
struct PairingParams
{
    QString subscriberId;
    QString serverBaseUrl;
    QString registrationUrl;
    QString pairingToken;
    QString deviceName;
};

// Wraps NativeRegistrationClient with the "persist on success, leave
// untouched otherwise" policy from Linux_QT_Client_Plan.md's Phase 4 scope:
// PairingStore and SettingsStore's delivery fields only ever change together,
// and only on RegistrationOutcome::Success.
class DeviceRegistrationService
{
public:
    DeviceRegistrationService(NativeRegistrationClient& client, PairingStore& pairingStore,
                               SettingsStore& settingsStore);

    NativeRegistrationResult pair(const PairingParams& params, const QString& deviceToken);

    std::optional<NativeRegistrationResult> reregisterIfPaired(const QString& deviceToken);

private:
    NativeRegistrationClient& m_client;
    PairingStore& m_pairingStore;
    SettingsStore& m_settingsStore;
};
