#pragma once

#include "net/NetworkError.h"

#include <QString>
#include <QUrl>

class HttpClient;

enum class DeregisterOutcome
{
    Success,
    Unauthorized,
    Failure,
};

struct DeregisterResult
{
    DeregisterOutcome outcome = DeregisterOutcome::Failure;
    QString detail; // meaningful only on Failure
};

// Removes this device from the account's paired-devices list via POST
// {serverBaseUrl}/api/notifications/native/deregister. Verified against
// internal/api/server.go's handleNotificationNativeDeregister — the device
// authenticates via X-Kypost-Device-Id/X-Kypost-Device-Secret headers
// (RelayAuth), same as every other authenticated Relay endpoint; the body
// is empty. Unauthorized (401) means the credentials are already invalid --
// callers should treat that the same as a successful removal (see
// PairingController::removePairing()).
class DeregisterClient
{
public:
    explicit DeregisterClient(HttpClient& httpClient);

    DeregisterResult deregister(const QUrl& serverBaseUrl, const QString& deviceId, const QString& deviceSecret) const;

private:
    HttpClient& m_httpClient;
};
