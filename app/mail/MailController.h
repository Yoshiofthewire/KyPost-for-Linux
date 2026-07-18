#pragma once

#include "mail/EmailListModel.h"
#include "models/Email.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <optional>

class MailRepository;
class RelayMailSource;
class KeywordRepository;
class PairingStore;
struct RelayAuth;
class QUrl;

// QML-facing bridge (Task 32) over the core/domain mail stack: MailRepository
// (cache + delta-merge), RelayMailSource (direct relay calls for actions/
// send/attachments -- MailRepository itself only wraps fetchInbox), and
// KeywordRepository (per-folder keyword tab derivation). Registered as the
// "MailApp" QML singleton in main.cpp. Every method here that reaches the
// network runs synchronously on the calling (GUI) thread -- see Phase 6
// global constraint 2, this is a known, accepted freeze-the-UI tradeoff for
// this phase, not a bug.
//
// Task 39: allKeywordSettings()/setKeywordVisible() (Settings > Keywords
// pane) are folded in here rather than a new KeywordSettingsController --
// this class already holds the keywordRepository reference and a cached
// email set to derive the keyword universe from, so a second controller
// would just be a thin pass-through with no state of its own.
class MailController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* emailModel READ emailModel CONSTANT)
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY currentFolderChanged)
    Q_PROPERTY(QString selectedKeyword READ selectedKeyword NOTIFY selectedKeywordChanged)
    Q_PROPERTY(QVariantList keywordTabs READ keywordTabs NOTIFY keywordTabsChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    MailController(MailRepository& mailRepository, RelayMailSource& relayMailSource,
                    KeywordRepository& keywordRepository, PairingStore& pairingStore, QObject* parent = nullptr);

    QObject* emailModel() const;
    QString currentFolder() const;
    QString selectedKeyword() const; // "" = All
    QVariantList keywordTabs() const; // [{name, count}, ...] -- "All" NOT included, see Task 38
    bool isBusy() const;
    QString lastError() const; // "" when none

public slots:
    // Sets currentFolder, resets selectedKeyword to "", loads the on-disk
    // cache for the new folder so the model has something to show
    // immediately, then calls refresh() to hit the network.
    void selectFolder(const QString& wireFolder);
    // Re-filters the model from the already-cached folder emails. No
    // network call.
    void selectKeyword(const QString& keyword);
    void refresh(bool forceFullResync = false);
    bool archiveEmails(const QStringList& messageIds);
    bool deleteEmails(const QStringList& messageIds);
    bool markSpam(const QStringList& messageIds);
    bool moveEmails(const QStringList& messageIds, const QString& targetFolder);
    // mode is hardcoded "html" -- Compose.qml's RichBodyEditor is the sole
    // caller and always produces sanitized HTML (see
    // docs/superpowers/specs/2026-07-18-html-compose-design.md). Reads each
    // local file path via QFile, derives mimeType via QMimeDatabase, and
    // enforces a 25 MB total-bytes cap across all attachments before calling
    // relayMailSource.sendMail -- returns false + sets lastError (without
    // truncating) if the cap is exceeded.
    bool sendMail(const QString& to, const QString& cc, const QString& bcc, const QString& subject,
                  const QString& body, const QStringList& attachmentFilePaths);
    QVariantList listAttachments(const QString& mailbox, const QString& messageId); // [{index, name, mimeType, size}, ...]
    // Writes the downloaded bytes to QStandardPaths::DownloadLocation,
    // deduping the filename against anything already there. Returns false +
    // sets lastError on failure.
    bool downloadAttachment(const QString& mailbox, const QString& messageId, int index,
                             const QString& suggestedName);
    QVariantList standardFolders() const; // [{wireName, displayName}, ...], enum order
    // Task 35: looks up a single cached Email by messageId (independent of
    // folder, see MailRepository::findCachedEmail) as a QVariantMap keyed
    // the same way as EmailListModel::roleNames() (messageId/folder/sender/
    // sentTo/cc/bcc/subject/preview/body/label/keywords/status/atUtc/
    // hasAttachments/sourceMode), so EmailDetail.qml can bind to
    // result.subject/result.sender/etc. the same way it already binds to
    // emailModel row properties. Returns an empty map if the messageId
    // isn't cached locally -- this is a pure local-cache read (no network
    // call), so a miss just means "not fetched/cached yet", not an error.
    Q_INVOKABLE QVariantMap findByMessageId(const QString& messageId) const;
    // Task 39: Settings > Keywords pane. Wraps keywordRepository.allSettings()
    // over the Inbox's cached emails specifically (m_mailRepository.
    // cachedEmails("INBOX")), NOT m_currentFolderEmails/whatever folder is
    // currently selected -- this is the one deliberate exception to "read
    // from the already-filtered current folder" elsewhere in this class,
    // since a Settings screen should show a stable keyword universe
    // independent of whatever folder the mail list happens to be showing.
    // Known limitation (matches the task-39 brief's own note): a keyword
    // only ever seen on emails in folders OTHER than Inbox won't appear
    // here -- deriving the universe from every folder's cache would need a
    // repository method this task doesn't add (Global Constraint 7). Result
    // shape: [{keyword, visible}, ...], alphabetical (case-insensitive),
    // same ordering as keywordTabs()/allSettings() itself.
    Q_INVOKABLE QVariantList allKeywordSettings() const;
    // Task 39: KeywordRepository::setVisible() -- also re-emits
    // keywordTabsChanged() since toggling a keyword's visibility can change
    // whether it appears in the currently-filtered folder's keyword tab row
    // (see keywordTabs()'s own doc comment).
    Q_INVOKABLE void setKeywordVisible(const QString& keyword, bool visible);

signals:
    void currentFolderChanged();
    void selectedKeywordChanged();
    void keywordTabsChanged();
    void isBusyChanged();
    void lastErrorChanged();
    // Task 42: forwarded straight from NotificationDispatcher::openRequested
    // (main.cpp connects the two directly -- signal-to-signal, no lambda,
    // since the shapes already match) when the user activates a
    // notification's "View" action. MailController itself has no window/
    // pageStack access (same constraint every other controller in this repo
    // respects -- see PairingController's deep-link routing for the closest
    // precedent), so this only carries the bare messageId to QML;
    // MobileRoot.qml/DesktopRoot.qml each have a
    // `Connections { target: MailApp }` block that hydrates the full email
    // via findByMessageId() and does the actual navigation + window
    // raise/focus.
    void openEmailRequested(const QString& messageId);

private:
    void applyFilter(); // recomputes m_model from m_currentFolderEmails + m_selectedKeyword
    void setBusy(bool busy);
    void setLastError(const QString& error);
    // Loads pairing state via m_pairingStore.load() into serverBaseUrl/auth.
    // Returns false (and sets lastError to "Not paired") without touching
    // either out-param when there is no saved pairing -- every network-
    // calling method below short-circuits on this before making a request.
    bool requirePairing(QUrl& serverBaseUrl, RelayAuth& auth);
    // Shared body of archiveEmails/deleteEmails/markSpam/moveEmails.
    bool performActionCommon(const QStringList& messageIds, const QString& action,
                              const std::optional<QString>& targetMailbox);
    static QString dedupedFilePath(const QString& directory, const QString& fileName);

    MailRepository& m_mailRepository;
    RelayMailSource& m_relayMailSource;
    KeywordRepository& m_keywordRepository;
    PairingStore& m_pairingStore;
    EmailListModel* m_model; // owned, parented to this
    QVector<Email> m_currentFolderEmails; // last cachedEmails(currentFolder) result, pre-keyword-filter
    QString m_currentFolder = QStringLiteral("INBOX");
    QString m_selectedKeyword;
    bool m_isBusy = false;
    QString m_lastError;
};
