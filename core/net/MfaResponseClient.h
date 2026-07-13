#pragma once

#include "net/NetworkError.h"

#include <QString>
#include <QUrl>
#include <optional>

class HttpClient;

// Mirrors llama-Mail-for-Mac's MfaResponseOutcome shape (Data/Networking/
// MfaResponseClient.swift) — that enum grouping is still accurate even
// though the request/response wire shapes it was built against are stale
// (see MfaResponseClient below). Rejected covers both "the server refused
// this device's credentials" (401/403) and "this challenge was already
// resolved" (409) — Swift groups both under one outcome because the caller
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
// handlePushRespond (see Task 15 brief) — the Swift reference client's
// {challengeId, approved} body with sub/hash as query params is stale and
// was NOT used as a source for this shape; this endpoint takes no
// query-param auth, unlike every other Relay endpoint in this batch, and
// the boolean key on the wire is "approve", not "approved".
class MfaResponseClient
{
public:
    explicit MfaResponseClient(HttpClient& httpClient);

    MfaResponseResult respond(const QUrl& serverBaseUrl, const QString& challengeId, const QString& subscriberId,
                               const QString& subscriberHash, const QString& deviceId, bool approve) const;

private:
    HttpClient& m_httpClient;
};
