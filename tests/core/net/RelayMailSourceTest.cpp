#include "net/RelayMailSource.h"

#include "models/Email.h"
#include "net/HttpClient.h"
#include "net/RelayAuth.h"

#include "FakeRelayServer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTest>

class RelayMailSourceTest : public QObject
{
    Q_OBJECT

private slots:
    void fetchInboxMapsTwoTabsWithAtUtcPassthroughAndOptionalFields();
    void fetchInboxSendsLimitMailboxSinceAndAuthAsQueryParams();
    void fetchInboxOmitsLimitAndSinceWhenNotProvided();
    void fetchInboxUnauthorizedFrom401PassesErrorThrough();

    void listFoldersSendsParentAndAuthAsQueryParamsAndParsesResult();
    void createFolderSendsParentNameBodyAndParsesStringFolderResult();
    void renameFolderSendsPutWithFolderNameBodyAndParsesResult();
    void deleteFolderSendsDeleteWithFolderQueryParamNotBody();

    void performActionMoveIncludesTargetMailboxInRequestBody();
    void performActionReadOmitsTargetMailboxFromRequestBodyButResponseCarriesEmptyString();
};

void RelayMailSourceTest::fetchInboxMapsTwoTabsWithAtUtcPassthroughAndOptionalFields()
{
    const QByteArray body = R"(
    {
      "tabs": ["Inbox", "Archive"],
      "byTab": {
        "Inbox": [
          {
            "messageId": "m1",
            "sender": "alice@example.com",
            "sentTo": "bob@example.com",
            "cc": "cc@example.com",
            "bcc": "bcc@example.com",
            "subject": "Hello",
            "body": "Body text",
            "status": "unread",
            "atUtc": "2026-07-01T12:00:00Z",
            "hasAttachments": true,
            "label": "important",
            "detail": "queued",
            "changeType": "updated"
          }
        ],
        "Archive": [
          {
            "messageId": "m2",
            "sender": "carol@example.com",
            "sentTo": "dave@example.com",
            "cc": "",
            "bcc": "",
            "subject": "Archived",
            "status": "read",
            "atUtc": "2026-06-01T08:30:00Z",
            "hasAttachments": false,
            "label": ""
          }
        ]
      }
    }
    )";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const InboxFetchResult result = source.fetchInbox(serverBaseUrl, auth, 100, QStringLiteral("Inbox"), qint64(0));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.tabs, QStringList({ QStringLiteral("Inbox"), QStringLiteral("Archive") }));
    QCOMPARE(result.byTab.size(), 2);

    QVERIFY(result.byTab.contains(QStringLiteral("Inbox")));
    QCOMPARE(result.byTab.value(QStringLiteral("Inbox")).size(), 1);
    const InboxEmailItem& item1 = result.byTab.value(QStringLiteral("Inbox")).at(0);
    QCOMPARE(item1.email.messageId, QStringLiteral("m1"));
    QCOMPARE(item1.email.sender, QStringLiteral("alice@example.com"));
    QCOMPARE(item1.email.sentTo, QStringLiteral("bob@example.com"));
    QCOMPARE(item1.email.cc, QStringLiteral("cc@example.com"));
    QCOMPARE(item1.email.bcc, QStringLiteral("bcc@example.com"));
    QCOMPARE(item1.email.subject, QStringLiteral("Hello"));
    QVERIFY(item1.email.body.has_value());
    QCOMPARE(*item1.email.body, QStringLiteral("Body text"));
    // No distinct "preview" key exists on the wire (confirmed against the Go
    // backend) -- preview must stay empty, not be populated from body.
    QVERIFY(item1.email.preview.isEmpty());
    QCOMPARE(item1.email.status, QStringLiteral("unread"));
    // atUtc is a direct pass-through of the wire key of the same name -- no
    // casing translation, assert it is unchanged.
    QCOMPARE(item1.email.atUtc, QStringLiteral("2026-07-01T12:00:00Z"));
    QCOMPARE(item1.email.hasAttachments, true);
    QCOMPARE(item1.email.label, QStringLiteral("important"));
    // folder is set from the enclosing byTab map key, not a wire field.
    QCOMPARE(item1.email.folder, QStringLiteral("Inbox"));
    QCOMPARE(item1.detail, QStringLiteral("queued"));
    QVERIFY(item1.changeType.has_value());
    QCOMPARE(*item1.changeType, QStringLiteral("updated"));

    QVERIFY(result.byTab.contains(QStringLiteral("Archive")));
    QCOMPARE(result.byTab.value(QStringLiteral("Archive")).size(), 1);
    const InboxEmailItem& item2 = result.byTab.value(QStringLiteral("Archive")).at(0);
    QCOMPARE(item2.email.messageId, QStringLiteral("m2"));
    QCOMPARE(item2.email.folder, QStringLiteral("Archive"));
    // "body"/"detail"/"changeType" absent from the wire -> nullopt/empty, not
    // a parse error.
    QVERIFY(!item2.email.body.has_value());
    QVERIFY(item2.detail.isEmpty());
    QVERIFY(!item2.changeType.has_value());
}

void RelayMailSourceTest::fetchInboxSendsLimitMailboxSinceAndAuthAsQueryParams()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"tabs":[],"byTab":{}})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-9"), QStringLiteral("hash-9") };
    source.fetchInbox(serverBaseUrl, auth, 250, QStringLiteral("Inbox"), qint64(12345));

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/inbox?"));
    QVERIFY(request.contains("sub=sub-9"));
    QVERIFY(request.contains("hash=hash-9"));
    QVERIFY(request.contains("limit=250"));
    QVERIFY(request.contains("mailbox=Inbox"));
    QVERIFY(request.contains("since=12345"));
}

void RelayMailSourceTest::fetchInboxOmitsLimitAndSinceWhenNotProvided()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"tabs":[],"byTab":{}})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    source.fetchInbox(serverBaseUrl, auth, std::nullopt, QStringLiteral("Inbox"), std::nullopt);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(!request.contains("limit="));
    QVERIFY(!request.contains("since="));
    QVERIFY(request.contains("mailbox=Inbox"));
}

void RelayMailSourceTest::fetchInboxUnauthorizedFrom401PassesErrorThrough()
{
    FakeRelayServer fake(httpResponse(401, "Unauthorized", "Unauthorized\n"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const InboxFetchResult result = source.fetchInbox(serverBaseUrl, auth, std::nullopt, QStringLiteral("Inbox"), std::nullopt);

    QVERIFY(result.error.has_value());
    QCOMPARE(*result.error, NetworkError::Unauthorized);
    QVERIFY(result.byTab.isEmpty());
}

void RelayMailSourceTest::listFoldersSendsParentAndAuthAsQueryParamsAndParsesResult()
{
    const QByteArray body = R"(
    {
      "parent": "Inbox",
      "folders": [
        {"path": "Inbox/Work", "deletable": true},
        {"path": "Inbox/Personal", "deletable": false}
      ]
    }
    )";
    FakeRelayServer fake(httpResponse(200, "OK", body));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ListFoldersResult result = source.listFolders(serverBaseUrl, auth, QStringLiteral("Inbox"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.parent, QStringLiteral("Inbox"));
    QCOMPARE(result.folders.size(), 2);
    QCOMPARE(result.folders.at(0).path, QStringLiteral("Inbox/Work"));
    QCOMPARE(result.folders.at(0).deletable, true);
    QCOMPARE(result.folders.at(1).path, QStringLiteral("Inbox/Personal"));
    QCOMPARE(result.folders.at(1).deletable, false);

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("GET /api/inbox/folders?"));
    QVERIFY(request.contains("parent=Inbox"));
    QVERIFY(request.contains("sub=sub-1"));
    QVERIFY(request.contains("hash=hash-1"));
}

void RelayMailSourceTest::createFolderSendsParentNameBodyAndParsesStringFolderResult()
{
    // POST response's "folder" key is a plain string path here (mailClient.
    // CreateFolder returns (string, error)) -- NOT the {path, deletable}
    // object shape used by the GET list response.
    FakeRelayServer fake(
        httpResponse(200, "OK", R"({"ok":true,"parent":"Inbox","name":"NewFolder","folder":"Inbox/NewFolder"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const CreateFolderResult result =
        source.createFolder(serverBaseUrl, auth, QStringLiteral("Inbox"), QStringLiteral("NewFolder"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.parent, QStringLiteral("Inbox"));
    QCOMPARE(result.name, QStringLiteral("NewFolder"));
    QCOMPARE(result.folder, QStringLiteral("Inbox/NewFolder"));

    QVERIFY(fake.receivedRequest().contains("POST /api/inbox/folders?"));
    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("parent")).toString(), QStringLiteral("Inbox"));
    QCOMPARE(sent.value(QStringLiteral("name")).toString(), QStringLiteral("NewFolder"));
}

void RelayMailSourceTest::renameFolderSendsPutWithFolderNameBodyAndParsesResult()
{
    FakeRelayServer fake(
        httpResponse(200, "OK", R"({"ok":true,"folder":"Inbox/Old","renamed":"Inbox/New","parent":"Inbox"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const RenameFolderResult result =
        source.renameFolder(serverBaseUrl, auth, QStringLiteral("Inbox/Old"), QStringLiteral("New"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.folder, QStringLiteral("Inbox/Old"));
    QCOMPARE(result.renamed, QStringLiteral("Inbox/New"));
    QCOMPARE(result.parent, QStringLiteral("Inbox"));

    QVERIFY(fake.receivedRequest().contains("PUT /api/inbox/folders?"));
    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("folder")).toString(), QStringLiteral("Inbox/Old"));
    QCOMPARE(sent.value(QStringLiteral("name")).toString(), QStringLiteral("New"));
}

void RelayMailSourceTest::deleteFolderSendsDeleteWithFolderQueryParamNotBody()
{
    FakeRelayServer fake(httpResponse(200, "OK", R"({"ok":true,"folder":"Inbox/Old","parent":"Inbox"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const DeleteFolderResult result = source.deleteFolder(serverBaseUrl, auth, QStringLiteral("Inbox/Old"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.folder, QStringLiteral("Inbox/Old"));
    QCOMPARE(result.parent, QStringLiteral("Inbox"));

    const QByteArray request = fake.receivedRequest();
    QVERIFY(request.contains("DELETE /api/inbox/folders?"));
    QVERIFY(request.contains("folder="));
    // The folder target travels as a query param, not a JSON body.
    QVERIFY(!request.contains("Content-Length:"));
}

void RelayMailSourceTest::performActionMoveIncludesTargetMailboxInRequestBody()
{
    FakeRelayServer fake(
        httpResponse(200, "OK", R"({"ok":true,"action":"move","processed":2,"failed":[],"targetMailbox":"Archive"})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ActionResult result = source.performAction(serverBaseUrl, auth, QStringLiteral("move"),
                                                       { QStringLiteral("m1"), QStringLiteral("m2") },
                                                       QStringLiteral("Inbox"), QStringLiteral("Archive"));

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.ok, true);
    QCOMPARE(result.action, QStringLiteral("move"));
    QCOMPARE(result.processed, 2);
    QVERIFY(result.failed.isEmpty());
    QCOMPARE(result.targetMailbox, QStringLiteral("Archive"));

    QVERIFY(fake.receivedRequest().contains("POST /api/inbox/actions?"));
    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("action")).toString(), QStringLiteral("move"));
    QCOMPARE(sent.value(QStringLiteral("mailbox")).toString(), QStringLiteral("Inbox"));
    QCOMPARE(sent.value(QStringLiteral("messageIds")).toArray().size(), 2);
    QVERIFY(sent.contains(QStringLiteral("targetMailbox")));
    QCOMPARE(sent.value(QStringLiteral("targetMailbox")).toString(), QStringLiteral("Archive"));
}

void RelayMailSourceTest::performActionReadOmitsTargetMailboxFromRequestBodyButResponseCarriesEmptyString()
{
    // targetMailbox is ALWAYS present on the wire response (even as "" when
    // the action wasn't "move") -- but must never be sent in the *request*
    // body for a non-move action.
    FakeRelayServer fake(httpResponse(
        200, "OK",
        R"({"ok":true,"action":"read","processed":0,"failed":[{"messageId":"m3","error":"not found"}],"targetMailbox":""})"));
    QNetworkAccessManager manager;
    HttpClient http(manager);
    RelayMailSource source(http);

    const QUrl serverBaseUrl(QStringLiteral("http://127.0.0.1:%1").arg(fake.port()));
    const RelayAuth auth{ QStringLiteral("sub-1"), QStringLiteral("hash-1") };
    const ActionResult result = source.performAction(serverBaseUrl, auth, QStringLiteral("read"),
                                                       { QStringLiteral("m3") }, QStringLiteral("Inbox"), std::nullopt);

    QVERIFY(!result.error.has_value());
    QCOMPARE(result.action, QStringLiteral("read"));
    QCOMPARE(result.processed, 0);
    // targetMailbox is present in the response as "", parsed as such -- not
    // an absent/default value being confused with "omitted".
    QCOMPARE(result.targetMailbox, QString());
    QVERIFY(result.targetMailbox.isEmpty());
    QCOMPARE(result.failed.size(), 1);
    QCOMPARE(result.failed.at(0).messageId, QStringLiteral("m3"));
    QCOMPARE(result.failed.at(0).error, QStringLiteral("not found"));

    const QJsonObject sent = fake.receivedJsonBody();
    QCOMPARE(sent.value(QStringLiteral("action")).toString(), QStringLiteral("read"));
    QVERIFY(!sent.contains(QStringLiteral("targetMailbox")));
}

QTEST_GUILESS_MAIN(RelayMailSourceTest)
#include "RelayMailSourceTest.moc"
