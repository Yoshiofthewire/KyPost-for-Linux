#include "push/UnifiedPushConnector.h"
#include "theme/ThemeController.h"

#include "stores/SettingsStore.h"

#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
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

// Task 12 stub router: scans a set of command-line-style arguments for a
// llamalabels:// deep link and logs it. Real pairing-URL parsing (token
// extraction, routing to a QML page, etc.) is a later phase -- this proof
// only needs to demonstrate that the URL reaches application code, whether
// via a fresh launch's argv or via KDBusService::activateRequested relaying
// a second launch's argv to the already-running instance.
static void routeDeepLink(const QStringList& arguments)
{
    for (const QString& argument : arguments) {
        const QUrl url(argument);
        if (url.scheme() == QStringLiteral("llamalabels")) {
            // Only scheme+host+path are logged, never the query string. Today this URL
            // only ever carries a test token, but the plan's real contract is
            // llamalabels://native-pair?sub=...&hash=...&pt=... -- subscriber id, subscriber
            // hash, and pairing token, all real authentication credentials once native
            // pairing lands. Logging the full URL now would set a precedent that leaks
            // those to the journal later, so strip the query up front instead.
            QUrl redacted = url;
            redacted.setQuery(QString());
            qDebug() << "StubRouter: received deep link:" << redacted;
            return;
        }
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
    KDBusService dbusService(KDBusService::Unique);
    QObject::connect(&dbusService, &KDBusService::activateRequested, &dbusService,
                      [](const QStringList& arguments, const QString& workingDirectory) {
                          qDebug() << "KDBusService: activateRequested -- arguments:" << arguments
                                    << "workingDirectory:" << workingDirectory;
                          // Task 12: a second `llamamail llamalabels://...` invocation gets
                          // redirected here instead of spawning a duplicate process -- route
                          // its argv through the same stub deep-link handler used at startup.
                          routeDeepLink(arguments);
                      });

    // Task 12: this process is the one that "won" KDBusService's Unique-mode
    // registration, so also check its own argv for a llamalabels:// URL --
    // covers the case where xdg-open launches llamamail fresh (nothing was
    // running yet to redirect to).
    routeDeepLink(app.arguments());

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
