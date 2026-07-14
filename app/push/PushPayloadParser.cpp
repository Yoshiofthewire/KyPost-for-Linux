#include "push/PushPayloadParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace PushPayloadParser {

namespace {

// Same split-on-comma convention as core/net/PushNotificationClient.cpp's
// splitKeywords -- data's Keywords wire value is comma-joined (capital K).
// Qt::SkipEmptyParts drops the empties a leading/trailing/doubled comma
// would otherwise produce ("" from "work,,urgent," -> "work", "urgent").
QStringList splitKeywords(const QString& commaJoined)
{
    if (commaJoined.isEmpty())
        return {};

    QStringList result;
    for (const QString& part : commaJoined.split(QLatin1Char(','), Qt::SkipEmptyParts))
        result.append(part.trimmed());
    return result;
}

} // namespace

std::optional<PushNotification> parse(const QByteArray& rawBody)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(rawBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;

    // A missing "data" object yields an empty QJsonObject here, which in
    // turn yields an empty messageId below -- falls through to the same
    // nullopt path as an explicitly missing messageId, no separate check
    // needed.
    const QJsonObject data = doc.object().value(QStringLiteral("data")).toObject();

    PushNotification notification;
    notification.messageId = data.value(QStringLiteral("messageId")).toString();
    if (notification.messageId.isEmpty())
        return std::nullopt;

    notification.sender = data.value(QStringLiteral("sender")).toString();
    notification.subject = data.value(QStringLiteral("subject")).toString();
    notification.senderName = data.value(QStringLiteral("senderName")).toString();
    notification.emailSubject = data.value(QStringLiteral("emailSubject")).toString();
    notification.keywords = splitKeywords(data.value(QStringLiteral("Keywords")).toString());
    // data's own title/body copies are authoritative here -- see this file's
    // header comment for why that differs from PushNotificationClient.cpp's
    // pull-mode parsing.
    notification.title = data.value(QStringLiteral("title")).toString();
    notification.body = data.value(QStringLiteral("body")).toString();
    notification.url = data.value(QStringLiteral("url")).toString();

    return notification;
}

} // namespace PushPayloadParser
