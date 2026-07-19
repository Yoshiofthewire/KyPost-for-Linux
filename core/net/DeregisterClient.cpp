#include "net/DeregisterClient.h"

#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include <QJsonObject>

DeregisterClient::DeregisterClient(HttpClient& httpClient)
    : m_httpClient(httpClient)
{
}

DeregisterResult DeregisterClient::deregister(const QUrl& serverBaseUrl, const QString& deviceId,
                                               const QString& deviceSecret) const
{
    const RelayAuth auth{ deviceId, deviceSecret };
    const HttpClient::HttpResult result = m_httpClient.post(
        joinUrlPath(serverBaseUrl, QStringLiteral("api/notifications/native/deregister")), {}, QJsonObject{},
        auth.headerItems());

    DeregisterResult out;

    if (result.error.has_value()) {
        switch (*result.error) {
        case NetworkError::Unauthorized:
            out.outcome = DeregisterOutcome::Unauthorized;
            return out;
        default:
            out.outcome = DeregisterOutcome::Failure;
            out.detail = !result.detail.isEmpty()
                ? result.detail
                : QStringLiteral("Deregister request failed with status %1").arg(result.statusCode);
            return out;
        }
    }

    out.outcome = DeregisterOutcome::Success;
    return out;
}
