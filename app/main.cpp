#include "contacts/ContactsController.h"
#include "mail/MailController.h"
#include "pairing/MfaController.h"
#include "pairing/PairingController.h"
#include "platform/SecureStoreKeychain.h"
#include "push/UnifiedPushConnector.h"
#include "theme/ThemeController.h"

#include "db/ContactDao.h"
#include "db/Database.h"
#include "db/EmailDao.h"
#include "db/FolderDao.h"
#include "db/PendingContactChangeDao.h"
#include "db/PushDao.h"
#include "domain/ContactSyncRepository.h"
#include "domain/DeviceRegistrationService.h"
#include "domain/KeywordRepository.h"
#include "domain/MailRepository.h"
#include "domain/PairingStore.h"
#include "net/ContactSyncClient.h"
#include "net/HttpClient.h"
#include "net/MfaResponseClient.h"
#include "net/NativeRegistrationClient.h"
#include "net/RelayMailSource.h"
#include "stores/CursorStore.h"
#include "stores/SettingsStore.h"

#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QUrl>

#include <KDBusService>

// Task 28: every bundled font file (see app/resources/fonts.qrc) loaded via
// QFontDatabase::addApplicationFont before the QQmlApplicationEngine is
// constructed, so QML content can reference these families from the very
// first frame. These 11 files are baked into the resource bundle at build
// time (app/resources/fonts.qrc), so a load failure here is a packaging
// defect, not an expected runtime condition -- unlike the mobile
// counterparts' system-font-fallback pattern, there is no silent fallback:
// qWarning() on every -1 so a broken bundle is loud, not silently degraded.
static void loadBundledFonts()
{
    static const char* const kFontResources[] = {
        ":/fonts/SpaceGrotesk-Light.ttf",
        ":/fonts/SpaceGrotesk-Regular.ttf",
        ":/fonts/SpaceGrotesk-Medium.ttf",
        ":/fonts/SpaceGrotesk-SemiBold.ttf",
        ":/fonts/SpaceGrotesk-Bold.ttf",
        ":/fonts/IBMPlexMono-Regular.ttf",
        ":/fonts/IBMPlexMono-Italic.ttf",
        ":/fonts/IBMPlexMono-Medium.ttf",
        ":/fonts/IBMPlexMono-SemiBold.ttf",
        ":/fonts/IBMPlexMono-Bold.ttf",
        ":/fonts/IBMPlexMono-BoldItalic.ttf",
    };
    for (const char* resource : kFontResources) {
        const int id = QFontDatabase::addApplicationFont(QString::fromLatin1(resource));
        if (id == -1) {
            qWarning() << "loadBundledFonts: failed to load bundled font:" << resource;
            continue;
        }
        qDebug() << "loadBundledFonts: loaded" << resource << "as"
                  << QFontDatabase::applicationFontFamilies(id);
    }
}

// Task 34: replaces the Task 12 log-only stub -- scans a set of
// command-line-style arguments for a llamalabels:// deep link and, for a
// recognized llamalabels://native-pair link, actually routes it to
// PairingController::pairFromDeepLink() instead of just logging it (see
// PairingController.cpp's parseNativePairLink() for the full sub/hash/srv/
// pt/reg parsing contract). Reachable both via a fresh launch's argv and via
// KDBusService::activateRequested relaying a second launch's argv to the
// already-running instance -- same dual call-site shape Task 12 established.
//
// llamalabels://desktop-pair (or any other llamalabels:// host) is
// unrecognized here, per Phase 6 global constraint 6 (desktop pairing is
// out of scope for this client family) -- it falls through to the same
// redacted-query-string qDebug() Task 11's security review required,
// unchanged. Never logs sub/hash/pt (or the query string of any
// unrecognized llamalabels:// link) -- those are real authentication
// credentials.
//
// pairingController is a plain (non-owning) pointer rather than a
// reference: see main()'s comment on pairingControllerForDeepLinks for why
// it can transiently be nullptr between KDBusService's construction and
// PairingController's.
static void routeDeepLink(const QStringList& arguments, PairingController* pairingController)
{
    for (const QString& argument : arguments) {
        const QUrl url(argument);
        if (url.scheme() != QStringLiteral("llamalabels"))
            continue;

        if (url.host() == QStringLiteral("native-pair")) {
            if (pairingController == nullptr) {
                // Should not happen in practice -- see main()'s comment on
                // pairingControllerForDeepLinks -- but a dropped pairing
                // attempt is much better than a null dereference.
                qWarning() << "routeDeepLink: native-pair link arrived before PairingController was ready, dropping";
                return;
            }
            qDebug() << "routeDeepLink: routing llamalabels://native-pair link to PairingController";
            pairingController->pairFromDeepLink(url);
            return;
        }

        // Any other llamalabels:// host (including desktop-pair, which
        // Phase 6 global constraint 6 puts explicitly out of scope) is
        // unrecognized -- strip the query string before logging, same as
        // the Task 11 stub did, so a real credential never lands in the
        // journal.
        QUrl redacted = url;
        redacted.setQuery(QString());
        qDebug() << "routeDeepLink: received unrecognized deep link:" << redacted;
        return;
    }
}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    // Task 11: applicationName + organizationDomain feed KDBusService's name
    // derivation (reversed domain + app name -- see KDBusService::KDBusService
    // docs), which must produce the same "com.urlxl.LlamaMail" well-known
    // name that UnifiedPushConnector registers below and that the .service
    // file at ~/.local/share/dbus-1/services/com.urlxl.LlamaMail.service
    // advertises to the D-Bus daemon for on-demand activation.
    app.setApplicationName(QStringLiteral("LlamaMail"));
    app.setOrganizationDomain(QStringLiteral("urlxl.com"));

    // KDBusService (KF6DBusAddons, confirmed installed via `pacman -Qs
    // kdbusaddons`) claims the well-known bus name in Unique mode. Verified
    // (Task 11) that this build (KF6DBusAddons 6.27.0) does NOT export
    // org.freedesktop.Application/Activate at /MainApplication -- introspection
    // shows only org.qtproject.Qt.QCoreApplication/QGuiApplication reflected
    // properties there. activateRequested() below fires via the documented
    // Unique-mode collision path instead: launching the binary again while an
    // instance is already running (including one the D-Bus daemon just spawned
    // per the .service file's Exec=) relays the new invocation's argv/cwd to
    // the running instance over D-Bus and emits this signal there; the
    // duplicate process then quits on its own. Constructed before
    // QQmlApplicationEngine per KDBusService's own race-avoidance guidance:
    // export D-Bus objects before the event loop (app.exec()) runs.
    //
    // Must also stay constructed before UnifiedPushConnector below: this is
    // what actually claims the "com.urlxl.LlamaMail" well-known bus name on
    // the session bus. UnifiedPushConnector relies on that name already
    // being owned by this same connection by the time it constructs -- it
    // does not register the name itself. Reordering these two would make
    // UnifiedPushConnector grab the name first, and KDBusService::Unique
    // would then think another instance is already running and relay-and-quit
    // on every launch.
    //
    // Task 34: PairingController doesn't exist yet at this point in main()
    // (it's constructed further down, after the core/domain composition
    // root it wraps) but the lambda below needs to be able to reach it once
    // a second launch is relayed here -- which can only happen once this
    // process has entered app.exec(), by which point main() has long since
    // finished constructing pairingController and pointed
    // pairingControllerForDeepLinks at it. The lambda captures this pointer
    // variable by reference (not the not-yet-existing controller itself),
    // so it always sees the up-to-date value whenever activateRequested
    // actually fires.
    PairingController* pairingControllerForDeepLinks = nullptr;
    KDBusService dbusService(KDBusService::Unique);
    QObject::connect(&dbusService, &KDBusService::activateRequested, &dbusService,
                      [&pairingControllerForDeepLinks](const QStringList& arguments, const QString& workingDirectory) {
                          // Task 34 security fix: this used to log the raw `arguments` list
                          // (Task 11/12), which is fine while every llamalabels:// URL only
                          // ever carries a fake test token, but arguments can now carry a
                          // real llamalabels://native-pair link -- sub/hash/pt are real
                          // authentication credentials once pairFromDeepLink() below
                          // succeeds. Log only the count and workingDirectory here;
                          // routeDeepLink() below does its own properly-redacted logging of
                          // the link itself.
                          qDebug() << "KDBusService: activateRequested -- argument count:" << arguments.size()
                                    << "workingDirectory:" << workingDirectory;
                          // Task 12/34: a second `llamamail llamalabels://...` invocation gets
                          // redirected here instead of spawning a duplicate process -- route
                          // its argv through the same deep-link router used at startup.
                          routeDeepLink(arguments, pairingControllerForDeepLinks);
                      });

    // Task 28: bundled fonts must be registered with QFontDatabase before
    // the QQmlApplicationEngine below parses any QML that might reference
    // them by family name (none does yet, but ThemeController.fontUi()/
    // fontMono() already advertise these family names to future QML).
    loadBundledFonts();

    // Task 28: first real on-disk location for app-level persistent state
    // (core/ deliberately never decides this -- see SettingsStore.h's doc
    // comment). AppConfigLocation is the XDG-correct place for a
    // human-editable-if-needed settings file, distinct from AppDataLocation
    // (databases/caches, not used by this task). Constructed before
    // ThemeController/QQmlApplicationEngine since both need it to already
    // exist.
    const QString settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(settingsDir);
    SettingsStore settingsStore(settingsDir + QStringLiteral("/settings.ini"));

    // Task 28: wraps core::ThemeManager (which itself wraps settingsStore
    // above) for QML consumption. Constructed before the engine and
    // registered as a QML singleton instance immediately after, so
    // MobileRoot.qml (loaded just below) can already bind to Theme.* from
    // its very first frame in a later task -- this task only proves the
    // singleton registers and loads cleanly.
    ThemeController themeController(settingsStore);
    qmlRegisterSingletonInstance<ThemeController>(
        "com.urlxl.LlamaMail", 1, 0, "Theme", &themeController);

    // Task 31: composition root for the rest of core/db, core/net, and
    // core/domain -- every later Phase 6 task (32-34) builds its
    // QObject-derived controller against references into this graph rather
    // than constructing its own copies. Everything below is a main() local,
    // same lifetime tier as settingsStore/themeController above (declared
    // before QQmlApplicationEngine, destroyed in reverse declaration order
    // at the end of main()). Order matters: each object below only takes
    // references/handles to objects already constructed above it.
    //
    // 1. Database -- AppDataLocation (not AppConfigLocation, which
    // settingsDir above already claimed for settings.ini) is the XDG-correct
    // place for a real on-disk database file. A failed open() has no
    // reasonable degraded mode -- every DAO below hands out a live
    // QSqlDatabase& into this object, so treat it as fatal.
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    Database database;
    if (!database.open(dataDir + QStringLiteral("/llamamail.db")))
        qFatal("main: Database::open failed for %s", qPrintable(dataDir + QStringLiteral("/llamamail.db")));

    // 2. DAOs, each borrowing database.handle(). FolderDao/PushDao have no
    // Phase 6 caller yet -- constructed anyway per the task-31 brief, since
    // they're part of the schema Phase 2 already built and future phases
    // will need them wired here rather than re-deriving this block.
    EmailDao emailDao(database.handle());
    ContactDao contactDao(database.handle());
    FolderDao folderDao(database.handle());
    PushDao pushDao(database.handle());
    PendingContactChangeDao pendingContactChangeDao(database.handle());

    // 3. SecureStoreKeychain -- the app/platform/ Secret-Service-backed
    // SecureStore built in Phase 2 (SecureStoreFile is for tests/UT only).
    // Reuses the same "com.urlxl.LlamaMail" service name KDBusService and
    // UnifiedPushConnector already use above, for consistency.
    SecureStoreKeychain secureStore(QStringLiteral("com.urlxl.LlamaMail"));

    // 4. CursorStore -- reuses settingsDir (already computed for
    // SettingsStore above), not a second directory-resolution block.
    CursorStore cursorStore(settingsDir + QStringLiteral("/cursors.ini"));

    // 5. PairingStore -- the one shared "are we paired" contract every
    // repository below reads instead of re-deriving SecureStore key names.
    PairingStore pairingStore(secureStore);

    // 6. HttpClient -- default transferTimeoutMs. networkManager must
    // outlive httpClient and every net/ client below that borrows it
    // transitively.
    QNetworkAccessManager networkManager;
    HttpClient httpClient(networkManager);

    // 7. core/net clients -- thin wire-format wrappers around httpClient.
    RelayMailSource relayMailSource(httpClient);
    ContactSyncClient contactSyncClient(httpClient);
    MfaResponseClient mfaResponseClient(httpClient);
    NativeRegistrationClient nativeRegistrationClient(httpClient);

    // 8. core/domain repositories -- the layer Tasks 32-34's QML-facing
    // controllers actually call into.
    MailRepository mailRepository(relayMailSource, emailDao, pairingStore, cursorStore);
    KeywordRepository keywordRepository(settingsStore);
    ContactSyncRepository contactSyncRepository(contactSyncClient, contactDao, pendingContactChangeDao,
                                                 cursorStore, pairingStore);
    DeviceRegistrationService deviceRegistrationService(nativeRegistrationClient, pairingStore, settingsStore);

    // 9. Deliberately NOT wired here: PushRepository/PushNotificationClient/
    // NtfySubscriber/TransportStateMachine. Settings->Notifications only
    // needs SettingsStore::deliveryMode()/transport() (already available
    // above); full push-arrival wiring is deferred to Phase 7 per the
    // Phase 4 final-review note.
    //
    // 10. Nothing above is registered with QML yet -- Tasks 32-34 each add
    // "construct controller X, register it" below, right above
    // QQmlApplicationEngine, without reordering anything in this block.

    // Task 32: QML-facing bridge over mailRepository/relayMailSource/
    // keywordRepository/pairingStore (all constructed above). Owns its
    // EmailListModel (parented to itself); every network-calling slot on
    // this controller blocks the GUI thread synchronously, same accepted
    // tradeoff as every other Phase 6 controller (see global constraint 2).
    MailController mailController(mailRepository, relayMailSource, keywordRepository, pairingStore);
    qmlRegisterSingletonInstance<MailController>(
        "com.urlxl.LlamaMail", 1, 0, "MailApp", &mailController);

    // Task 33: QML-facing bridge over contactSyncRepository (constructed
    // above). Owns its ContactListModel (parented to itself); sync() blocks
    // the GUI thread synchronously, same accepted tradeoff as MailController
    // above (see global constraint 2). Its model starts empty until QML
    // calls load()/sync() -- see ContactsController's constructor comment.
    ContactsController contactsController(contactSyncRepository);
    qmlRegisterSingletonInstance<ContactsController>(
        "com.urlxl.LlamaMail", 1, 0, "ContactsApp", &contactsController);

    // Task 34: QML-facing bridge over deviceRegistrationService/pairingStore
    // (both constructed above). Refreshes its isPaired/pairedServerHost/
    // deviceId from pairingStore eagerly on construction (see its
    // constructor comment) -- unlike Mail/ContactsController, there's no
    // reasonable "empty until QML asks" state for "are we paired".
    // Task 39: also takes settingsStore directly (constructed at the very
    // top of main()) so its read-only deliveryMode()/transport()/
    // pushServerBaseUrl() properties (Settings > Notifications) can read
    // straight from it -- see PairingController.h's doc comment on why
    // those three reuse pairingChanged() rather than a new signal.
    PairingController pairingController(deviceRegistrationService, pairingStore, settingsStore);
    qmlRegisterSingletonInstance<PairingController>(
        "com.urlxl.LlamaMail", 1, 0, "Pairing", &pairingController);

    // pairingController now exists -- point the pointer the KDBusService
    // activateRequested lambda above captured (by reference) at it, so a
    // second launch relaying a llamalabels://native-pair link over D-Bus can
    // actually reach PairingController. See that lambda's comment for why
    // this ordering is safe.
    pairingControllerForDeepLinks = &pairingController;

    // Task 34: this process is the one that "won" KDBusService's Unique-mode
    // registration (construction above already guarantees that -- a losing
    // instance relays its argv and exits at that point, it doesn't reach
    // here), so also check its own argv for a llamalabels:// URL -- covers
    // the case where xdg-open launches llamamail fresh (nothing was running
    // yet to redirect to). Moved here (versus immediately after KDBusService,
    // where the Task 12 stub ran this) so PairingController already exists
    // to route a native-pair link to.
    routeDeepLink(app.arguments(), pairingControllerForDeepLinks);

    // Task 34: QML-facing bridge over mfaResponseClient/pairingStore (both
    // constructed above).
    MfaController mfaController(mfaResponseClient, pairingStore);
    qmlRegisterSingletonInstance<MfaController>(
        "com.urlxl.LlamaMail", 1, 0, "Mfa", &mfaController);

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qml/MobileRoot.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    // Task 10 proof-of-concept: exercise the KUnifiedPush connector so its
    // endpoint/state/message signals can be observed via qDebug(). Not wired
    // into real push handling yet -- see app/push/UnifiedPushConnector.h.
    UnifiedPushConnector pushConnector(QStringLiteral("com.urlxl.LlamaMail"));
    pushConnector.registerClient(QStringLiteral("Llama Mail push notifications"));

    return app.exec();
}
