#include "models/StandardFolder.h"

#include <QStringList>

QString standardFolderWireName(StandardFolder folder)
{
    switch (folder) {
    case StandardFolder::Inbox:
        return QStringLiteral("INBOX");
    case StandardFolder::Drafts:
        return QStringLiteral("Drafts");
    case StandardFolder::Junk:
        return QStringLiteral("Junk");
    case StandardFolder::Sent:
        return QStringLiteral("Sent");
    case StandardFolder::Trash:
        return QStringLiteral("Trash");
    case StandardFolder::Archive:
        return QStringLiteral("Archive");
    }
    return QString();
}

std::optional<StandardFolder> standardFolderFromWireName(const QString& wireName)
{
    if (wireName == QStringLiteral("INBOX"))
        return StandardFolder::Inbox;
    if (wireName == QStringLiteral("Drafts"))
        return StandardFolder::Drafts;
    if (wireName == QStringLiteral("Junk"))
        return StandardFolder::Junk;
    if (wireName == QStringLiteral("Sent"))
        return StandardFolder::Sent;
    if (wireName == QStringLiteral("Trash"))
        return StandardFolder::Trash;
    if (wireName == QStringLiteral("Archive"))
        return StandardFolder::Archive;
    return std::nullopt;
}

QString standardFolderDisplayName(const QString& fullPath)
{
    const QString normalized = QString(fullPath).replace(QLatin1Char('.'), QLatin1Char('/'));
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return parts.isEmpty() ? fullPath : parts.last();
}
