#include "net/NtfySubscriber.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

NtfySubscriber::NtfySubscriber(QNetworkAccessManager& manager, const QString& baseUrl, const QString& topic,
                                QObject* parent, int reconnectDelayMs)
    : QObject(parent)
    , m_manager(manager)
    , m_baseUrl(baseUrl)
    , m_topic(topic)
    , m_reconnectDelayMs(reconnectDelayMs)
{
}

void NtfySubscriber::start(qint64 since)
{
    m_since = since;
    m_stopped = false;
    sendRequest();
}

void NtfySubscriber::stop()
{
    m_stopped = true;
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void NtfySubscriber::sendRequest()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_lineBuffer.clear();

    QString base = m_baseUrl;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);

    QUrl url(base + QLatin1Char('/') + m_topic + QStringLiteral("/json"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("since"), QString::number(m_since));
    url.setQuery(query);

    m_reply = m_manager.get(QNetworkRequest(url));
    connect(m_reply, &QNetworkReply::readyRead, this, &NtfySubscriber::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &NtfySubscriber::onFinished);
}

void NtfySubscriber::onReadyRead()
{
    if (!m_reply)
        return;

    m_lineBuffer += m_reply->readAll();

    int newlineIndex;
    while ((newlineIndex = m_lineBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_lineBuffer.left(newlineIndex);
        m_lineBuffer.remove(0, newlineIndex + 1);
        processLine(line);
    }
}

void NtfySubscriber::onFinished()
{
    if (!m_reply)
        return;

    QNetworkReply* reply = m_reply;
    m_reply = nullptr;

    const QString reason = reply->errorString();
    reply->deleteLater();

    if (m_stopped)
        return;

    emit connectionLost(reason);

    // m_since was already advanced to the last processed message's `time`
    // by processLine(), so the retry below resumes from there rather than
    // from 0 -- no messages are lost across a reconnect.
    QTimer::singleShot(m_reconnectDelayMs, this, [this]() {
        if (!m_stopped)
            sendRequest();
    });
}

void NtfySubscriber::processLine(const QByteArray& line)
{
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(trimmed);
    if (!doc.isObject())
        return;

    const QJsonObject obj = doc.object();
    // "open"/"keepalive" lines exist only to hold the long-poll connection
    // open and carry no payload -- only "message" is a real notification
    // (docs.ntfy.sh/subscribe/api/#json-stream).
    if (obj.value(QStringLiteral("event")).toString() != QStringLiteral("message"))
        return;

    if (obj.contains(QStringLiteral("time")))
        m_since = static_cast<qint64>(obj.value(QStringLiteral("time")).toDouble());

    emit messageReceived(obj);
}
