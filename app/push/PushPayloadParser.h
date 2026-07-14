#pragma once

#include "models/PushNotification.h"

#include <QByteArray>
#include <optional>

// Parses the raw UnifiedPush message body (JSON bytes, as delivered by
// KUnifiedPush::Connector::messageReceived -- see UnifiedPushConnector.cpp)
// into a PushNotification. Pure function over bytes, no QObject needed, so
// this is a plain namespace rather than a class -- matches the task-40 brief.
//
// Verified wire envelope (backend/internal/processor/native_sender.go +
// poller.go's buildNativePushData, cross-checked in
// .superpowers/sdd/phase7-global-constraints.md):
//   { "title": "...", "body": "...",
//     "data": { "messageId", "sender", "subject", "senderName",
//               "emailSubject", "Keywords" (comma-joined, capital K),
//               "title", "body", "url" } }
//
// PushNotification::title/body are populated from data.title/data.body, NOT
// the outer envelope's title/body -- the outer pair exists purely for
// distributor/OS-tray display before this parser ever runs; data's copy is
// the source of truth for this app's own model. (This is the opposite
// convention from PushNotificationClient.cpp's pull-mode parsing, which
// takes title/body from the item's top level -- that envelope shape has no
// data.title/data.body duplicate. Two different wire shapes, deliberately
// handled differently; see PushNotificationClient.cpp's own comment.)
namespace PushPayloadParser {

// Returns std::nullopt on malformed JSON, a non-object top level, or a
// missing/empty data.messageId -- messageId is the one field every other
// piece of this repo (PushDao/PushRepository::recordPushArrival) treats as
// the required identity key.
std::optional<PushNotification> parse(const QByteArray& rawBody);

} // namespace PushPayloadParser
