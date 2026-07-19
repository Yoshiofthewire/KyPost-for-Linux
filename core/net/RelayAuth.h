#pragma once

#include <QList>
#include <QPair>
#include <QString>

// Relay auth credentials (subscriber id + hash), sent as
// X-Kypost-Subscriber-Id/X-Kypost-Subscriber-Hash headers on every
// authenticated Relay request. Mirrors llama-Mail-for-Mac's RelayAuth
// (Data/Networking/HTTPClient.swift), read for structure only. Plain value
// type with no store dependency -- callers pull sub/hash out of whatever
// pairing/session store owns them and hand this to HttpClient::get/post.
struct RelayAuth
{
    QString sub;
    QString hash;

    QList<QPair<QString, QString>> headerItems() const
    {
        return { { QStringLiteral("X-Kypost-Subscriber-Id"), sub },
                 { QStringLiteral("X-Kypost-Subscriber-Hash"), hash } };
    }

    bool operator==(const RelayAuth&) const = default;
};
