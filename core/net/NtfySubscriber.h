#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// The one QObject-derived class in core/net/: a long-poll GET against an
// ntfy.sh-compatible JSON-stream endpoint (docs.ntfy.sh/subscribe/api/
// #json-stream) legitimately stays open and delivers data as it arrives, so
// it needs signals rather than HttpClient's synchronous-via-QEventLoop
// request/reply shape every other core/net/ client uses (see
// Linux_QT_Client_Plan.md's "Embedded ntfy subscriber" section, lines
// ~204-216). No Android/Swift reference exists to port from -- Android only
// has a UnifiedPush distributor path, no embedded ntfy client -- this is new
// design based on the plan doc and ntfy's own published wire shape.
class NtfySubscriber : public QObject
{
    Q_OBJECT
public:
    // baseUrl e.g. "https://ntfy.sh" (SettingsStore::pushServerBaseUrl()) or
    // a self-hosted equivalent -- never hardcode ntfy.sh here, per the
    // custom-backend design decision in Linux_QT_Client_Plan.md.
    // reconnectDelayMs mirrors HttpClient's transferTimeoutMs pattern: a
    // constructor-overridable knob so tests don't have to wait out the real
    // 5s production default to exercise the reconnect path.
    NtfySubscriber(QNetworkAccessManager& manager, const QString& baseUrl, const QString& topic,
                    QObject* parent = nullptr, int reconnectDelayMs = 5000);

    // Starts (or restarts) the long-poll GET to
    // {baseUrl}/{topic}/json?since={since}. since is a unix-seconds
    // timestamp or ntfy message id -- the resume point after a reconnect;
    // 0 means "only new messages from now on" (ntfy's own since=0 default
    // semantics -- 0 is not translated into "all history").
    void start(qint64 since = 0);
    void stop();

signals:
    // One emission per "event":"message" line -- this class only demuxes
    // the ntfy envelope; mapping ntfy's {title,message} (or a nested `data`
    // object) onto core/models/PushNotification is Task 25 / a later
    // phase's job.
    void messageReceived(const QJsonObject& data);

    // Connection dropped or the HTTP request itself failed (non-2xx,
    // transport error). Also fired for the long-poll connection's own
    // server-side timeout close -- v1 does not distinguish "expected
    // timeout" from "real failure". NtfySubscriber reconnects on its own
    // after reconnectDelayMs using the last successfully-processed
    // message's `time` as the new since, so no messages are lost.
    void connectionLost(const QString& reason);

private slots:
    void onReadyRead();
    void onFinished();

private:
    void sendRequest();
    void processLine(const QByteArray& line);

    QNetworkAccessManager& m_manager;
    QString m_baseUrl;
    QString m_topic;
    int m_reconnectDelayMs;
    QNetworkReply* m_reply = nullptr;
    qint64 m_since = 0;
    QByteArray m_lineBuffer; // partial line across onReadyRead calls
    bool m_stopped = true;
};
