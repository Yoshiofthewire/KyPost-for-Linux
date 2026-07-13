#include "pairing/MfaController.h"

#include "domain/DevicePairing.h"
#include "domain/PairingStore.h"
#include "net/MfaResponseClient.h"

#include <QUrl>

MfaController::MfaController(MfaResponseClient& client, PairingStore& pairingStore, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_pairingStore(pairingStore)
{
}

QString MfaController::respondState() const
{
    return m_respondState;
}

QString MfaController::resultMessage() const
{
    return m_resultMessage;
}

void MfaController::setRespondState(const QString& state, const QString& message)
{
    if (m_respondState == state && m_resultMessage == message)
        return;
    m_respondState = state;
    m_resultMessage = message;
    emit respondStateChanged();
}

void MfaController::respond(const QString& challengeId, bool approve)
{
    const std::optional<DevicePairing> pairing = m_pairingStore.load();
    if (!pairing.has_value()) {
        setRespondState(QStringLiteral("failed"), QStringLiteral("Not paired"));
        return;
    }

    setRespondState(QStringLiteral("sending"));

    const MfaResponseResult result = m_client.respond(QUrl(pairing->serverBaseUrl), challengeId,
                                                        pairing->subscriberId, pairing->subscriberHash,
                                                        pairing->deviceId, approve);

    switch (result.outcome) {
    case MfaResponseOutcome::Success:
        setRespondState(QStringLiteral("done"), approve ? QStringLiteral("Approved") : QStringLiteral("Denied"));
        break;
    case MfaResponseOutcome::Rejected:
        // status is populated from the response body when the server
        // included one (always on Success, optionally on a 409 Rejected --
        // see MfaResponseResult's doc comment); its presence is what lets
        // us tell "this challenge was already resolved" apart from "this
        // device's credentials were refused" (401/403, no status).
        if (result.status.has_value() && !result.status->isEmpty()) {
            setRespondState(QStringLiteral("failed"),
                             QStringLiteral("This request was already resolved (%1).").arg(*result.status));
        } else {
            setRespondState(QStringLiteral("failed"),
                             QStringLiteral("This request was already handled or denied."));
        }
        break;
    case MfaResponseOutcome::Failure:
        setRespondState(QStringLiteral("failed"),
                         result.detail.has_value() && !result.detail->isEmpty()
                             ? *result.detail
                             : QStringLiteral("Failed to send response, please try again."));
        break;
    }
}

void MfaController::reset()
{
    setRespondState(QStringLiteral("idle"));
}
