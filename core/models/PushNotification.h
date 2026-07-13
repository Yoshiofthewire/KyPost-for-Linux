#pragma once

#include <QString>
#include <QStringList>

// Fields from the backend's verified `buildNativePushData` envelope
// (backend/internal/processor/poller.go): the push `data` object carries
// `messageId`, `sender`, `subject`, `senderName`, `emailSubject`, a
// comma-joined `Keywords` string (capital K on the wire, parsed here into
// `keywords`), `title`, `body`, `url`.
struct PushNotification
{
    QString messageId;
    QString sender;
    QString subject;
    QString senderName;
    QString emailSubject;
    QStringList keywords;
    QString title;
    QString body;
    QString url;

    bool operator==(const PushNotification&) const = default;
};
