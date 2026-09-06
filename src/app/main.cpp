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
#include "ui/EditorSettings.h"
#include "ui/DebugModel.h"
#include "ui/ExtensionsModel.h"
#include "ui/LanguageModel.h"
#include "ui/RunPanelModel.h"
#include "ui/SettingsModel.h"
#include "ui/SourceControlModel.h"
#include "ui/UpdateModel.h"
#include "update/UpdateChecker.h"
#include "ui/Theme.h"
#include "buildrun/TaskRunner.h"
#include "debugger/DebugSession.h"
#include "extensions/ExtensionRegistry.h"
#include "langsvc/LanguageServiceManager.h"
#include "vcs/Repository.h"
#include "workspace/Workspace.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QDir>
#include <QIcon>
#include <QTimer>
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

/// Applies the interface scale before the application exists.
///
/// Qt reads QT_SCALE_FACTOR once, when the GUI application is constructed, and
/// the whole scene graph is laid out against it. There is no supported way to
/// restage that afterwards, so the setting is read from disk here - before
/// QGuiApplication - and a change takes effect on the next launch. The settings
/// page says so rather than presenting a slider that appears to do nothing.
///
/// Reading the file directly is deliberate: Settings needs no GUI, but the
/// object graph is built after the application for good reasons, and this one
/// value has to precede it.
void applyInterfaceScale()
{
    config::Settings settings;
    if (const core::Status status = config::SettingsStore::load(
            settings, config::Settings::Layer::User,
            config::SettingsStore::userSettingsPath());
        !status) {
        return;  // reported properly once logging and the real graph exist
    }

    const double scale = settings.doubleValue(QStringLiteral("accessibility.uiScale"));

    // 1.0 is the default; setting the variable anyway would override a scale the
    // user configured for their whole desktop.
    if (qFuzzyCompare(scale, 1.0) || scale <= 0.0) {
        return;
    }
    qputenv("QT_SCALE_FACTOR", QByteArray::number(scale));
}

} // namespace

int main(int argc, char* argv[])
{
    core::initializeLogging();

    applyInterfaceScale();

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

    // Fonts are registered before the QML engine starts, so the first frame is
    // already correct. Loading them later would show one frame in the fallback
    // face and then reflow, which reads as the application booting twice.
    //
    // A face that fails to register is reported and skipped rather than being
    // fatal: Keys still runs in the platform's own fonts, and a missing italic
    // should not stop someone editing a file.
    for (const QString& face : {
             QStringLiteral(":/fonts/Inter-Regular.otf"),
             QStringLiteral(":/fonts/Inter-SemiBold.otf"),
             QStringLiteral(":/fonts/Inter-Italic.otf"),
             QStringLiteral(":/fonts/JetBrainsMono-Regular.ttf"),
             QStringLiteral(":/fonts/JetBrainsMono-Medium.ttf"),
             QStringLiteral(":/fonts/JetBrainsMono-Bold.ttf"),
             QStringLiteral(":/fonts/JetBrainsMono-Italic.ttf"),
         }) {
        if (QFontDatabase::addApplicationFont(face) < 0) {
            qCWarning(lcUi) << "could not load bundled font" << face;
        }
    }

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
    ui::Theme theme(settings);

    ui::EditorSettings editorSettings(settings);

    workspace::Workspace workspace(settings, scheduler);
    if (const core::Status status = workspace.loadHistory(); !status) {
        qCWarning(lcCore) << "could not load recent projects:" << status.error().toString();
    }

    // Source control follows whatever project is open. It is its own object
    // rather than a member of Workspace: a project is not required to be a
    // repository, and the repository root is not always the project root.
    vcs::Repository repository(scheduler);

    QObject::connect(&workspace, &workspace::Workspace::projectOpened,
                     &repository, &vcs::Repository::openFor);
    QObject::connect(&workspace, &workspace::Workspace::projectClosed,
                     &repository, &vcs::Repository::close);

    // Anything touching the working tree can change the status, so the watcher
    // drives the refresh. Repository debounces, which is what makes it safe to
    // connect something this chatty to it.
    QObject::connect(&workspace.watcher(), &fs::FileWatcher::filesChanged,
                     &repository, [&repository] { repository.refresh(); });
    QObject::connect(&workspace.watcher(), &fs::FileWatcher::directoriesChanged,
                     &repository, [&repository] { repository.refresh(); });

    // Build and run. The runner owns no project state; the panel model reads
    // the project for its task defaults and its root.
    buildrun::TaskRunner taskRunner;

    // Language servers start on demand, per project. Launching every configured
    // one at startup would cost seconds and memory for languages the user never
    // opens.
    langsvc::LanguageServiceManager languageServices;

    // One session at a time. Two debuggees stopped in the same editor would be
    // ambiguous about which stack the user is looking at.
    debugger::DebugSession debugSession;

    // Extensions run out of process and are started only once every capability
    // they asked for has been granted.
    extensions::ExtensionRegistry extensionRegistry(commands, settings);

    QObject::connect(&workspace, &workspace::Workspace::projectOpened,
                     &extensionRegistry, &extensions::ExtensionRegistry::activateAll);
    QObject::connect(&workspace, &workspace::Workspace::projectClosed,
                     &extensionRegistry, &extensions::ExtensionRegistry::deactivateAll);

    QObject::connect(&workspace, &workspace::Workspace::projectOpened,
                     &languageServices, &langsvc::LanguageServiceManager::setProjectRoot);
    QObject::connect(&workspace, &workspace::Workspace::projectClosed,
                     &languageServices, [&languageServices] {
                         languageServices.setProjectRoot(QString());
                     });

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

    ui::EditorSettings::setInstance(&editorSettings);

    ui::SettingsModel settingsModel(settings);
    ui::SettingsModel::setInstance(&settingsModel);

    ui::SourceControlModel sourceControl(repository);
    ui::SourceControlModel::setInstance(&sourceControl);

    update::UpdateChecker updateChecker(settings);
    ui::UpdateModel updateModel(updateChecker);
    ui::UpdateModel::setInstance(&updateModel);

    ui::RunPanelModel runPanel(taskRunner, workspace.project());
    ui::RunPanelModel::setInstance(&runPanel);

    ui::LanguageModel language(languageServices, workspace.editors());
    ui::LanguageModel::setInstance(&language);

    ui::DebugModel debug(debugSession, workspace.project(), workspace.editors());
    ui::DebugModel::setInstance(&debug);

    ui::ExtensionsModel extensionsModel(extensionRegistry);
    ui::ExtensionsModel::setInstance(&extensionsModel);

    // A document reaches its server through here rather than the model
    // discovering it, so the sync cannot silently miss one.
    QObject::connect(&workspace, &workspace::Workspace::fileOpened, &app,
                     [&language, &workspace](const QString&) {
                         language.documentOpened(workspace.activeDocument());
                     });

    QQmlApplicationEngine engine;

    // The explorer's model is exposed directly rather than proxied through
    // AppController: QML's ListView needs the QAbstractItemModel itself, and
    // wrapping it would mean reimplementing the model interface for no gain.
    engine.rootContext()->setContextProperty(QStringLiteral("FileTree"),
                                             &workspace.fileTree());

    // One editor and one tab model per pane, kept bound to the layout. They are
    // created once and rebound as panes come and go: a context property that
    // appeared and disappeared would break bindings rather than re-evaluate.
    ui::EditorBindings editors(workspace.editors(), editorSettings);

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

    QObject::connect(&sourceControl, &ui::SourceControlModel::fileActivated,
                     &app, [&controller](const QString& absolutePath) {
                         controller.openFile(absolutePath);
                     });

    // A problem opens the file at the line the compiler named, the same path
    // the explorer and palette use.
    // Stopping at a breakpoint opens the file there, the same path everything
    // else in Keys uses to reach the editor.
    QObject::connect(&extensionRegistry, &extensions::ExtensionRegistry::notice, &app,
                     [&controller](const QString& message) {
                         controller.reportNotice(message);
                     });

    QObject::connect(&debug, &ui::DebugModel::locationRequested, &app,
                     [&controller](const QString& path, int line) {
                         controller.openFileAt(path, line, 1);
                     });

    QObject::connect(&debug, &ui::DebugModel::notice, &app,
                     [&controller](const QString& message) {
                         controller.reportNotice(message);
                     });

    QObject::connect(&language, &ui::LanguageModel::definitionFound, &app,
                     [&controller](const QString& path, int line, int column) {
                         controller.openFileAt(path, line + 1, column + 1);
                     });

    QObject::connect(&language, &ui::LanguageModel::notice, &app,
                     [&controller](const QString& message) {
                         controller.reportNotice(message);
                     });

    QObject::connect(&runPanel, &ui::RunPanelModel::problemActivated, &app,
                     [&controller](const QString& path, int line, int column) {
                         controller.openFileAt(path, line, column);
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

    // After the window is on screen. Startup time is the first thing anyone
    // judges an editor by, and a network request has no business in it.
    QTimer::singleShot(4000, &app, [&updateChecker] { updateChecker.checkIfDue(); });

    return app.exec();
}
