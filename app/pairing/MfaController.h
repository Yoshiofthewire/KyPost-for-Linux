#pragma once

#include <QObject>
#include <QString>

class MfaResponseClient;
class PairingStore;

// QML-facing bridge (Task 34) over core/net's MfaResponseClient, reading the
// authenticating sub/hash/deviceId straight out of PairingStore (this
// device's own MFA push-challenge responses are always sent as "this
// device", never on behalf of another). Registered as the "Mfa" QML
// singleton in main.cpp. respond() runs synchronously on the calling (GUI)
// thread -- see Phase 6 global constraint 2, this is a known, accepted
// freeze-the-UI tradeoff for this phase, not a bug.
class MfaController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString respondState READ respondState NOTIFY respondStateChanged) // "idle" | "sending" | "done" | "failed"
    Q_PROPERTY(QString resultMessage READ resultMessage NOTIFY respondStateChanged) // human-readable, meaningful for "done"/"failed"

public:
    MfaController(MfaResponseClient& client, PairingStore& pairingStore, QObject* parent = nullptr);

    QString respondState() const;
    QString resultMessage() const;

public slots:
    // Reads sub/hash/deviceId from pairingStore.load() -- if not paired,
    // respondState="failed"+resultMessage set, no network call. Otherwise
    // calls client.respond(serverBaseUrl, challengeId, sub, hash, deviceId,
    // approve) and maps the MfaResponseOutcome to respondState/
    // resultMessage: Success -> "done"; Rejected -> "failed" with a message
    // distinguishing "already resolved" when the server's status field
    // carried that information, else a generic denial message; Failure ->
    // "failed" with the detail.
    void respond(const QString& challengeId, bool approve);
    void reset(); // back to "idle", for a retry

signals:
    void respondStateChanged();

private:
    void setRespondState(const QString& state, const QString& message = QString());

    MfaResponseClient& m_client;
    PairingStore& m_pairingStore;
    QString m_respondState = QStringLiteral("idle");
    QString m_resultMessage;
};
