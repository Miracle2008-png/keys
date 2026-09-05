#include "ui/AppController.h"

#include "core/Log.h"

#include <QCoreApplication>
#include <QDateTime>

#include <algorithm>
#include <utility>

namespace keys::ui {
namespace {

constexpr auto kSidebarVisibleKey = "workbench.sidebarVisible";
constexpr auto kActiveViewKey = "workbench.activeView";
constexpr auto kSidebarWidthKey = "workbench.sidebarWidth";
constexpr auto kThemeKey = "appearance.theme";

// Mirrors Metrics.sidebarMinWidth / sidebarMaxWidth. Duplicated because config
// and ui/Metrics are different layers; the test suite asserts they agree.
constexpr int kSidebarMinWidth = 180;
constexpr int kSidebarMaxWidth = 600;

/// The instance main() publishes for QML; see Theme.cpp for the reasoning.
AppController* g_instance = nullptr;

/// Renders a timestamp the way the design's recent list does: "now",
/// "yesterday", "3d ago". Coarse on purpose - an exact time would be noise in a
/// list whose only job is to help the user recognise a project.
QString relativeTime(const QDateTime& when)
{
    if (!when.isValid()) {
        return {};
    }

    const qint64 seconds = when.secsTo(QDateTime::currentDateTime());
    if (seconds < 60) {
        return QCoreApplication::translate("AppController", "now");
    }
    if (seconds < 3600) {
        const int minutes = static_cast<int>(seconds / 60);
        return QCoreApplication::translate("AppController", "%1m ago").arg(minutes);
    }
    if (seconds < 86400) {
        const int hours = static_cast<int>(seconds / 3600);
        return QCoreApplication::translate("AppController", "%1h ago").arg(hours);
    }

    const int days = static_cast<int>(seconds / 86400);
    if (days == 1) {
        return QCoreApplication::translate("AppController", "yesterday");
    }
    return QCoreApplication::translate("AppController", "%1d ago").arg(days);
}

} // namespace

void AppController::setInstance(AppController* instance)
{
    g_instance = instance;
}

AppController* AppController::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Q_ASSERT_X(g_instance, "AppController::create",
               "AppController::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

AppController::AppController(core::CommandRegistry& commands,
                             config::Settings& settings,
                             config::AnimationPolicy& animation,
                             workspace::Workspace& workspace,
                             QObject* parent)
    : QObject(parent), m_commands(commands), m_settings(settings),
      m_animation(animation), m_workspace(workspace)
{
    connect(&m_workspace, &workspace::Workspace::projectOpened,
            this, &AppController::projectChanged);
    connect(&m_workspace, &workspace::Workspace::projectClosed,
            this, &AppController::projectChanged);
    connect(&m_workspace.recentProjects(), &workspace::RecentProjects::changed,
            this, &AppController::recentProjectsChanged);

    connect(&m_animation, &config::AnimationPolicy::changed,
            this, &AppController::animationChanged);

    // Workbench state lives in settings so it persists across sessions without a
    // second store to keep in sync. The UI re-reads on any relevant change.
    connect(&m_settings, &config::Settings::changed, this,
            [this](const QString& key, const QVariant&) {
                if (key == QLatin1String(kSidebarVisibleKey)
                    || key == QLatin1String(kActiveViewKey)
                    || key == QLatin1String(kSidebarWidthKey)) {
                    emit workbenchChanged();
                }
            });

    registerWorkbenchCommands();
}

bool AppController::sidebarVisible() const
{
    return m_settings.boolValue(QLatin1String(kSidebarVisibleKey));
}

QString AppController::activeView() const
{
    return m_settings.stringValue(QLatin1String(kActiveViewKey));
}

int AppController::sidebarWidth() const
{
    return m_settings.intValue(QLatin1String(kSidebarWidthKey));
}

bool AppController::invokeCommand(const QString& id)
{
    const core::Status status = m_commands.invoke(id);
    if (!status) {
        // A shortcut or button pointing at a command that does not exist is a
        // wiring bug; make it visible rather than a dead key.
        qCWarning(lcUi) << "command failed:" << status.error().toString();
        return false;
    }
    return true;
}

void AppController::selectView(const QString& view)
{
    // Clicking the active view collapses the sidebar, matching the design's
    // activity rail behaviour.
    if (view == activeView() && sidebarVisible()) {
        m_settings.setValue(QLatin1String(kSidebarVisibleKey), false);
        return;
    }

    m_settings.setValue(QLatin1String(kActiveViewKey), view);
    m_settings.setValue(QLatin1String(kSidebarVisibleKey), true);
}

void AppController::setSidebarWidth(int width)
{
    // Clamp here rather than in QML so every caller - drag handle, restored
    // session, a future command - gets the same bounds. A sidebar dragged to 8px
    // or past the window edge is a state the user cannot recover from.
    const int clamped = std::clamp(width, kSidebarMinWidth, kSidebarMaxWidth);
    m_settings.setValue(QLatin1String(kSidebarWidthKey), clamped);
}

QString AppController::projectName() const
{
    return m_workspace.hasProject() ? m_workspace.project().name() : QString();
}

QString AppController::projectRoot() const
{
    return m_workspace.hasProject() ? m_workspace.project().root() : QString();
}

bool AppController::hasProject() const
{
    return m_workspace.hasProject();
}

QVariantList AppController::recentProjects() const
{
    QVariantList result;
    const auto& entries = m_workspace.recentProjects().entries();
    result.reserve(entries.size());

    for (const workspace::RecentProject& entry : entries) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), entry.name);
        item.insert(QStringLiteral("path"), entry.path);
        item.insert(QStringLiteral("when"), relativeTime(entry.lastOpened));
        result.append(item);
    }
    return result;
}

bool AppController::openProject(const QString& path)
{
    const core::Status status = m_workspace.openProject(path);
    if (!status) {
        m_lastError = status.error().toString();
        qCWarning(lcUi) << "could not open project:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }

    m_lastError.clear();
    return true;
}

void AppController::closeProject()
{
    m_workspace.closeProject();
}

QString AppController::takeLastError()
{
    return std::exchange(m_lastError, QString());
}

void AppController::registerWorkbenchCommands()
{
    const auto add = [this](const QString& id, const QString& title,
                            const QString& category, std::function<void()> handler,
                            std::function<bool()> isEnabled = {}) {
        core::Command command;
        command.id = id;
        command.title = title;
        command.category = category;
        command.handler = std::move(handler);
        command.isEnabled = std::move(isEnabled);
        const core::Status status = m_commands.registerCommand(std::move(command));
        if (!status) {
            qCWarning(lcUi) << "failed to register command:" << status.error().toString();
        }
    };

    add(QStringLiteral("workspace.closeProject"),
        QStringLiteral("Close Project"),
        QStringLiteral("File"),
        [this] { closeProject(); },
        [this] { return hasProject(); });

    add(QStringLiteral("workbench.toggleSidebar"),
        QStringLiteral("Toggle Sidebar"),
        QStringLiteral("View"),
        [this] {
            m_settings.setValue(QLatin1String(kSidebarVisibleKey), !sidebarVisible());
        });

    add(QStringLiteral("workbench.toggleTheme"),
        QStringLiteral("Toggle Color Theme"),
        QStringLiteral("View"),
        [this] {
            const bool isDark =
                m_settings.stringValue(QLatin1String(kThemeKey)) != QLatin1String("light");
            m_settings.setValue(QLatin1String(kThemeKey),
                                isDark ? QStringLiteral("light") : QStringLiteral("dark"));
        });

    // The views the activity rail exposes. Registering them as commands means each
    // is reachable from the palette and bindable to a key without extra wiring.
    const QList<QPair<QString, QString>> views = {
        {QStringLiteral("explorer"), QStringLiteral("Explorer")},
        {QStringLiteral("search"), QStringLiteral("Search")},
        {QStringLiteral("sourceControl"), QStringLiteral("Source Control")},
        {QStringLiteral("debug"), QStringLiteral("Run and Debug")},
        {QStringLiteral("extensions"), QStringLiteral("Extensions")},
    };
    for (const auto& [id, title] : views) {
        add(QStringLiteral("view.show.%1").arg(id),
            QStringLiteral("Show %1").arg(title),
            QStringLiteral("View"),
            [this, id] { selectView(id); });
    }
}

} // namespace keys::ui
