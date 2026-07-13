#pragma once

#include <QString>
#include <optional>

enum class StandardFolder
{
    Inbox,
    Drafts,
    Junk,
    Sent,
    Trash,
    Archive,
};

QString standardFolderWireName(StandardFolder folder);
std::optional<StandardFolder> standardFolderFromWireName(const QString& wireName);

// Splits any folder path (not just the 6 standard folders) on both `/` and
// `.` hierarchy delimiters and returns the last segment.
QString standardFolderDisplayName(const QString& fullPath);
