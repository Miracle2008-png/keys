#include "config/AnimationPolicy.h"
#include "config/Settings.h"
#include "config/SettingsStore.h"
#include "core/CommandRegistry.h"
#include "core/Log.h"
#include "core/TaskScheduler.h"
#include "core/Trace.h"
#include "ui/AppController.h"
#include "ui/Theme.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStyleHints>

using namespace keys;

namespace {

/// Applies the operating system's colour-scheme preference on first run.
///
/// Only on first run: once the user has chosen a theme, that choice wins. A
/// system default should inform the initial state, not override an explicit one.
///
/// Motion is not handled here. Qt exposes no cross-platform reduced-motion hint,
/// and reading the Windows-specific setting is a platform integration that
/// belongs with the rest of the OS-preference work rather than bolted into main().
/// Until then `animation.level` defaults to "full" and the user controls it in
/// settings; AnimationPolicy::systemPrefersReducedMotion documents the seam.
void applySystemColorScheme(config::Settings& settings)
{
    if (settings.isSetIn(QStringLiteral("appearance.theme"), config::Settings::Layer::User)) {
        return;
    }

    const QStyleHints* hints = QGuiApplication::styleHints();
    if (!hints || hints->colorScheme() == Qt::ColorScheme::Unknown) {
        return;
    }

    const bool light = hints->colorScheme() == Qt::ColorScheme::Light;
    settings.setValue(QStringLiteral("appearance.theme"),
                      light ? QStringLiteral("light") : QStringLiteral("dark"));
}

} // namespace

int main(int argc, char* argv[])
{
    core::initializeLogging();

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Keys"));
    QGuiApplication::setApplicationVersion(QStringLiteral(KEYS_VERSION));

    // Deliberately no organization name: QStandardPaths joins organization and
    // application, so setting both to "Keys" would put settings in Keys/Keys/.
    // The domain is still set because Qt uses it for platform integration.
    QGuiApplication::setOrganizationDomain(QStringLiteral("keys.dev"));

    // Keys draws its own chrome from the design's tokens; a platform style would
    // fight it. Basic is the neutral, non-styling baseline.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    KEYS_TRACE("startup");

    // ---- Object graph ----------------------------------------------------
    // Constructed here and nowhere else. Modules receive references to what they
    // need rather than reaching for globals, which is what keeps them testable in
    // isolation and the dependency direction honest.
    core::TaskScheduler scheduler;
    core::CommandRegistry commands;
    config::Settings settings;

    if (const core::Status status = config::SettingsStore::load(
            settings, config::Settings::Layer::User,
            config::SettingsStore::userSettingsPath());
        !status) {
        // A malformed settings file must not prevent startup; the user needs a
        // running editor to fix it in.
        qCWarning(lcConfig) << "could not load user settings:" << status.error().toString();
    }

    applySystemColorScheme(settings);

    config::AnimationPolicy animation(settings);
    ui::Theme theme;
    theme.bindTo(settings);

    ui::AppController controller(commands, settings, animation);

    // ---- QML -------------------------------------------------------------
    // Publish the instances before the engine loads. The QML module declares
    // both as singletons with a create() factory that returns what is published
    // here, so QML sees the objects this function owns rather than constructing
    // its own unbound copies.
    ui::Theme::setInstance(&theme);
    ui::AppController::setInstance(&controller);

    QQmlApplicationEngine engine;

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] {
            qCCritical(lcUi) << "failed to create the root QML object";
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);

    engine.loadFromModule("Keys.Ui", "Main");

    // Persist settings on the way out rather than on every change: writing on each
    // keystroke of a slider would be pointless I/O.
    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&settings] {
        if (const core::Status status = config::SettingsStore::save(
                settings, config::Settings::Layer::User,
                config::SettingsStore::userSettingsPath());
            !status) {
            qCWarning(lcConfig) << "could not save settings:" << status.error().toString();
        }
    });

    return app.exec();
}
