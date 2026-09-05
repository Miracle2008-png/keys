#include "config/AnimationPolicy.h"
#include "config/Settings.h"
#include "config/SettingsStore.h"
#include "core/CommandRegistry.h"
#include "core/Log.h"
#include "core/TaskScheduler.h"
#include "core/Trace.h"
#include "ui/AppController.h"
#include "ui/CommandPaletteModel.h"
#include "ui/EditorBindings.h"
#include "ui/Theme.h"
#include "workspace/Workspace.h"

#include <QGuiApplication>
#include <QDir>
#include <QIcon>
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

    // Alt-Tab and the taskbar read this for the running process. On Windows the
    // executable also carries the icon as a resource, which is what Explorer and
    // pinned shortcuts use; both come from the same rendered mark.
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/branding/generated/keys-256.png")));

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

    workspace::Workspace workspace(settings, scheduler);
    if (const core::Status status = workspace.loadHistory(); !status) {
        qCWarning(lcCore) << "could not load recent projects:" << status.error().toString();
    }

    ui::AppController controller(commands, settings, animation, workspace);

    // A folder given on the command line opens at startup, so `keys .` behaves
    // the way a developer expects from a terminal.
    const QStringList arguments = QGuiApplication::arguments();
    if (arguments.size() > 1) {
        controller.openProject(arguments.at(1));
    }

    // ---- QML -------------------------------------------------------------
    // Publish the instances before the engine loads. The QML module declares
    // both as singletons with a create() factory that returns what is published
    // here, so QML sees the objects this function owns rather than constructing
    // its own unbound copies.
    ui::Theme::setInstance(&theme);
    ui::AppController::setInstance(&controller);

    ui::CommandPaletteModel palette(commands, workspace.fileIndex());
    ui::CommandPaletteModel::setInstance(&palette);

    QQmlApplicationEngine engine;

    // The explorer's model is exposed directly rather than proxied through
    // AppController: QML's ListView needs the QAbstractItemModel itself, and
    // wrapping it would mean reimplementing the model interface for no gain.
    engine.rootContext()->setContextProperty(QStringLiteral("FileTree"),
                                             &workspace.fileTree());

    // One editor and one tab model per pane, kept bound to the layout. They are
    // created once and rebound as panes come and go: a context property that
    // appeared and disappeared would break bindings rather than re-evaluate.
    ui::EditorBindings editors(workspace.editors());

    QObject::connect(&editors, &ui::EditorBindings::closeTabRequested, &app,
                     [&workspace](int group, int index) {
                         workspace.editors().setActiveGroup(group);
                         workspace.closeTab(index);
                     });

    // Choosing a file in the palette opens it, the same path the explorer uses.
    QObject::connect(&palette, &ui::CommandPaletteModel::fileChosen, &app,
                     [&workspace, &controller](const QString& relativePath) {
                         const QString absolute =
                             QDir(workspace.project().root()).filePath(relativePath);
                         controller.openFile(absolute);
                     });

    for (int i = 0; i < workspace::EditorLayout::kMaxGroups; ++i) {
        engine.rootContext()->setContextProperty(
            QStringLiteral("Editor%1").arg(i), editors.editorFor(i));
        engine.rootContext()->setContextProperty(
            QStringLiteral("Tabs%1").arg(i), editors.tabsFor(i));
    }

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
    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&settings, &workspace] {
        // Close the project first so its workspace settings are written while the
        // project root is still known.
        workspace.closeProject();

        if (const core::Status status = config::SettingsStore::save(
                settings, config::Settings::Layer::User,
                config::SettingsStore::userSettingsPath());
            !status) {
            qCWarning(lcConfig) << "could not save settings:" << status.error().toString();
        }
        if (const core::Status status = workspace.saveHistory(); !status) {
            qCWarning(lcCore) << "could not save recent projects:"
                              << status.error().toString();
        }
    });

    return app.exec();
}
