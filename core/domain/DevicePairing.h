#pragma once

#include <QString>

// The full set of pairing state PairingStore persists to SecureStore.
// subscriberId/subscriberHash/deviceId reuse SecureStore.h's own doc-comment
// key names ("sub"/"hash"/"deviceId"); the remaining four are prefixed
// "pairing." to avoid colliding with any future unrelated key.
struct DevicePairing
{
    QString subscriberId;    // SecureStore key "sub"
    QString subscriberHash;  // SecureStore key "hash" (may be empty)
    QString serverBaseUrl;   // SecureStore key "pairing.serverBaseUrl"
    QString registrationUrl; // SecureStore key "pairing.registrationUrl"
    QString pairingToken;    // SecureStore key "pairing.pairingToken"
    QString deviceId;        // SecureStore key "deviceId"
    QString deviceName;      // SecureStore key "pairing.deviceName"

    bool operator==(const DevicePairing&) const = default;
};
