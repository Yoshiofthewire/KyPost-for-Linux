#pragma once

#include "net/NetworkError.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

// Synchronous-from-the-caller's-point-of-view wrapper around
// QNetworkAccessManager for Relay HTTP calls. get()/post() block the calling
// thread on a local QEventLoop driven by the reply's finished signal — this
// mirrors llama-Mail-for-Mac's HTTPClient async/await call shape one-for-one
// (verified reference: Data/Networking/HTTPClient.swift, read for structure
// only), so every Task 14-18 client reads as a straight-line sequence
// instead of a signal/callback chain. Callers must invoke get()/post() off
// the GUI thread once app/ wiring exists in a later phase.
//
// The QNetworkAccessManager is injected via constructor reference rather
// than default-constructed internally, so tests can point it at a local
// QTcpServer and so threading/lifetime ownership stays with the caller.
class HttpClient
{
public:
    struct HttpResult
    {
        std::optional<NetworkError> error;
        int statusCode = 0;
        QByteArray body;
        QString detail; // human-readable detail for Transport/InvalidUrl failures; empty otherwise
    };

    explicit HttpClient(QNetworkAccessManager& manager);

    // HttpResult never decodes JSON: decoding into a concrete struct is each
    // Task 14-18 client's own responsibility (QJsonDocument::fromJson on
    // HttpResult::body, mapping a QJsonParseError to NetworkError::Decoding
    // if error is unset here but parsing still fails).
    HttpResult get(const QUrl& url, const QList<QPair<QString, QString>>& query,
                   const QList<QPair<QString, QString>>& headers = {});

    // Sets Content-Type: application/json.
    HttpResult post(const QUrl& url, const QList<QPair<QString, QString>>& query,
                     const QJsonObject& jsonBody, const QList<QPair<QString, QString>>& headers = {});

private:
    // Appends query items to url via QUrlQuery, preserving any query url
    // already has — mirrors the Swift URL.appending(queryOrThrow:) extension.
    QUrl urlWithQuery(const QUrl& url, const QList<QPair<QString, QString>>& query) const;

    HttpResult waitForReply(QNetworkReply* reply) const;

    QNetworkAccessManager& m_manager;
};
