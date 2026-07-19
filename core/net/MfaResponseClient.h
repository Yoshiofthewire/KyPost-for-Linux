#pragma once

#include "net/NetworkError.h"

#include <QString>
#include <QUrl>
#include <optional>

class HttpClient;

// Rejected covers both "the server refused this device's credentials"
// (401/403) and "this challenge was already resolved" (409) — the caller
// does the same thing either way (show a toast, don't retry).
enum class MfaResponseOutcome
{
    Success,
    Rejected,
    Failure,
};

// status is populated from the response body's "status" field when present
// (always on Success; optionally on a 409 Rejected). detail carries a
// human-readable failure reason and is meaningful only on Failure.
struct MfaResponseResult
{
    MfaResponseOutcome outcome = MfaResponseOutcome::Failure;
    std::optional<QString> status;
    std::optional<QString> detail;
};

// Responds to a push-based MFA challenge via POST {serverBaseUrl}/api/mfa/
// push/respond. Verified against internal/api/push_mfa_handlers.go's
// handlePushRespond — the device authenticates via X-Kypost-Device-Id/
// X-Kypost-Device-Secret headers (RelayAuth), same as every other
// authenticated Relay endpoint; the body carries only {challengeId,
// approve}.
class MfaResponseClient
{
public:
    explicit MfaResponseClient(HttpClient& httpClient);

    MfaResponseResult respond(const QUrl& serverBaseUrl, const QString& challengeId, const QString& deviceId,
                               const QString& deviceSecret, bool approve) const;

private:
    HttpClient& m_httpClient;
};
