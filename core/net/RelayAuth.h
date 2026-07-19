#pragma once

#include <QList>
#include <QPair>
#include <QString>

// Per-device auth credentials (deviceId + deviceSecret), sent as
// X-Kypost-Device-Id/X-Kypost-Device-Secret headers on every authenticated
// Relay request. deviceSecret is minted server-side once per successful
// registration and returned only in that response -- see
// DeviceRegistrationService::pair(). Plain value type with no store
// dependency -- callers pull deviceId/deviceSecret out of whatever
// pairing/session store owns them and hand this to HttpClient::get/post.
struct RelayAuth
{
    QString deviceId;
    QString deviceSecret;

    QList<QPair<QString, QString>> headerItems() const
    {
        return { { QStringLiteral("X-Kypost-Device-Id"), deviceId },
                 { QStringLiteral("X-Kypost-Device-Secret"), deviceSecret } };
    }

    bool operator==(const RelayAuth&) const = default;
};
