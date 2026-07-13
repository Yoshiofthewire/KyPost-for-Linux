#include "net/MfaResponseClient.h"

#include "net/HttpClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {

// Appends "api/mfa/push/respond" to serverBaseUrl's path, mirroring Swift's
// URL.appending(path:) — preserves any existing path on serverBaseUrl and
// ensures exactly one slash between the two, regardless of whether the
// caller's base URL was given with or without a trailing slash.
QUrl endpointFor(const QUrl& serverBaseUrl)
{
    QUrl url = serverBaseUrl;
    QString path = url.path();
    if (!path.endsWith(QLatin1Char('/')))
        path += QLatin1Char('/');
    path += QStringLiteral("api/mfa/push/respond");
    url.setPath(path);
    return url;
}

} // namespace

MfaResponseClient::MfaResponseClient(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

MfaResponseResult MfaResponseClient::respond(const QUrl& serverBaseUrl, const QString& challengeId,
                                              const QString& subscriberId, const QString& subscriberHash,
                                              const QString& deviceId, bool approve) const
{
    QJsonObject body;
    body[QStringLiteral("challengeId")] = challengeId;
    body[QStringLiteral("subscriberId")] = subscriberId;
    body[QStringLiteral("subscriberHash")] = subscriberHash;
    body[QStringLiteral("deviceId")] = deviceId;
    body[QStringLiteral("approve")] = approve;

    // No query-param auth on this endpoint, unlike every other Relay
    // endpoint in this batch — every credential rides in the JSON body.
    const HttpClient::HttpResult result = m_httpClient.post(endpointFor(serverBaseUrl), {}, body);

    MfaResponseResult out;

    if (result.error.has_value()) {
        switch (*result.error) {
        case NetworkError::Unauthorized:
            out.outcome = MfaResponseOutcome::Rejected;
            return out;
        case NetworkError::Conflict: {
            // 409 — challenge already resolved. status is optional here per
            // the brief; surface it when the server included it.
            out.outcome = MfaResponseOutcome::Rejected;
            const QJsonDocument doc = QJsonDocument::fromJson(result.body);
            if (doc.isObject()) {
                const QString status = doc.object().value(QStringLiteral("status")).toString();
                if (!status.isEmpty())
                    out.status = status;
            }
            return out;
        }
        default:
            out.outcome = MfaResponseOutcome::Failure;
            out.detail = !result.detail.isEmpty()
                ? result.detail
                : QStringLiteral("MFA response request failed with status %1").arg(result.statusCode);
            return out;
        }
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(result.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        out.outcome = MfaResponseOutcome::Failure;
        out.detail = QStringLiteral("Failed to decode MFA response: %1").arg(parseError.errorString());
        return out;
    }

    const QJsonObject json = doc.object();
    out.outcome = MfaResponseOutcome::Success;
    out.status = json.value(QStringLiteral("status")).toString();
    return out;
}
