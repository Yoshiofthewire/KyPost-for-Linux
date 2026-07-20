#pragma once

#include <QString>

// Inbound MFA push challenge shape, verified against kypost-android's
// MfaChallengePayload (app/src/main/java/com/urlxl/mail/push/
// MfaChallengePayload.kt): only `challengeId` arrives with the challenge —
// no message/description field exists on the wire today, so none is
// invented here. `deviceId`/`deviceSecret` needed to authenticate the
// `/api/mfa/push/respond` request live in a separate pairing/session store,
// not on this model. `receivedAt` is the local ISO-8601 UTC
// timestamp recorded when the challenge arrived (same QString convention as
// Email::atUtc), not itself a wire field.
struct MfaChallenge
{
    QString challengeId;
    QString receivedAt;

    bool operator==(const MfaChallenge&) const = default;
};
