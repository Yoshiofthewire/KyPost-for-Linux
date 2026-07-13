#pragma once

#include <QString>
#include <QStringList>
#include <optional>

// `atUtc` is kept as the raw wire ISO-8601 UTC string (matching the
// backend's `/api/inbox` `atUTC` field) rather than a Qt date/time type, to
// avoid parsing/timezone loss at the model layer.
struct Email
{
    QString messageId;
    QString folder;
    QString sender;
    QString sentTo;
    QString cc;
    QString bcc;
    QString subject;
    QString preview;
    std::optional<QString> body;
    QString label;
    QStringList keywords;
    QString status;
    QString atUtc;
    bool hasAttachments = false;
    QString sourceMode;

    bool operator==(const Email&) const = default;
};
