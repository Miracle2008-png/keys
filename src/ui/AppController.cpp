#include "ui/AppController.h"

#include <QDir>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileInfo>

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

    // Activating a row in the explorer opens it. The model signals intent; the
    // workspace performs the open, so the tree never touches file contents.
    connect(&m_workspace.fileTree(), &workspace::FileTreeModel::fileActivated,
            this, [this](const QString& path) { openFile(path); });

    connect(&m_workspace.editors(), &workspace::EditorLayout::groupsChanged,
            this, &AppController::editorsChanged);
    connect(&m_workspace.editors(), &workspace::EditorLayout::activeGroupChanged,
            this, &AppController::editorsChanged);

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

void AppController::setSettingsOpen(bool open)
{
    if (open == m_settingsOpen) {
        return;
    }
    m_settingsOpen = open;
    emit settingsOpenChanged();
}

QString AppController::activeView() const
{
    return m_settings.stringValue(QLatin1String(kActiveViewKey));
}

int AppController::sidebarWidth() const
{
    return m_settings.intValue(QLatin1String(kSidebarWidthKey));
}

void AppController::toggleTerminal()
{
    emit terminalToggleRequested();
}

void AppController::toggleProblems()
{
    emit problemsToggleRequested();
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

// ---- Tabs ------------------------------------------------------------------
//
// All of these act on the active group, which is the pane holding the caret.
// With the editor split, "close this tab" has to mean the one the user is
// looking at rather than whichever group happens to be first.

int AppController::tabCount() const
{
    const workspace::EditorGroup* group =
        m_workspace.editors().groupAt(m_workspace.editors().activeGroupIndex());
    return group ? group->tabCount() : 0;
}

bool AppController::hasUnsavedChanges() const
{
    return m_workspace.editors().hasUnsavedChanges();
}

bool AppController::hasOpenFile() const
{
    return m_workspace.activeDocument() != nullptr;
}

void AppController::closeOtherTabs()
{
    workspace::EditorGroup* group =
        m_workspace.editors().groupAt(m_workspace.editors().activeGroupIndex());
    if (!group || group->tabCount() < 2) {
        return;
    }

    // Backwards, and skipping the one being kept: closing forwards would
    // renumber every tab after each removal and take the wrong ones.
    const int keep = group->activeIndex();
    for (int i = group->tabCount() - 1; i >= 0; --i) {
        if (i != keep) {
            group->closeTab(i);
        }
    }
}

void AppController::closeAllTabs()
{
    workspace::EditorGroup* group =
        m_workspace.editors().groupAt(m_workspace.editors().activeGroupIndex());
    if (group) {
        group->closeAll();
    }
}

void AppController::nextTab()
{
    workspace::EditorGroup* group =
        m_workspace.editors().groupAt(m_workspace.editors().activeGroupIndex());
    if (!group || group->tabCount() < 2) {
        return;
    }
    // Wraps, the way every editor's Ctrl+Tab does.
    group->setActiveIndex((group->activeIndex() + 1) % group->tabCount());
}

void AppController::previousTab()
{
    workspace::EditorGroup* group =
        m_workspace.editors().groupAt(m_workspace.editors().activeGroupIndex());
    if (!group || group->tabCount() < 2) {
        return;
    }
    const int count = group->tabCount();
    group->setActiveIndex((group->activeIndex() - 1 + count) % count);
}

// ---- Files -----------------------------------------------------------------

bool AppController::saveAllFiles()
{
    const core::Status status = m_workspace.saveAllFiles();
    if (!status) {
        m_lastError = status.error().toString();
        emit errorOccurred(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

bool AppController::reloadActiveFile()
{
    const core::Status status = m_workspace.reloadActiveFile();
    if (!status) {
        m_lastError = status.error().toString();
        emit errorOccurred(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

bool AppController::switchHeaderSource()
{
    const editor::TextDocument* document = m_workspace.activeDocument();
    if (!document || document->path().isEmpty()) {
        return false;
    }

    const QFileInfo info(document->path());
    const QString suffix = info.suffix().toLower();
    const QString base = info.absolutePath() + QLatin1Char('/') + info.completeBaseName();

    // Tried in order, first hit wins. A project may use any of these and there
    // is no way to know which without looking.
    static const QStringList headers = {QStringLiteral("h"), QStringLiteral("hpp"),
                                        QStringLiteral("hh"), QStringLiteral("hxx")};
    static const QStringList sources = {QStringLiteral("cpp"), QStringLiteral("c"),
                                        QStringLiteral("cc"), QStringLiteral("cxx")};

    const QStringList& candidates = headers.contains(suffix) ? sources : headers;
    for (const QString& extension : candidates) {
        const QString path = base + QLatin1Char('.') + extension;
        if (QFileInfo::exists(path)) {
            return openFile(path);
        }
    }

    // Reported rather than silent: a shortcut that does nothing reads as
    // broken, and the honest answer is that the counterpart is not there.
    m_lastError = QStringLiteral("No counterpart file for %1").arg(info.fileName());
    emit errorOccurred(m_lastError);
    return false;
}

void AppController::goToLine(int line)
{
    const editor::TextDocument* document = m_workspace.activeDocument();
    if (!document) {
        return;
    }
    // Clamped: a line number past the end of the file should land at the end
    // rather than being refused, which is what every Go to Line does.
    const int target = std::clamp(line - 1, 0, document->lineCount() - 1);
    openFileAt(document->path(), target, 0);
}

void AppController::copyActivePath()
{
    const editor::TextDocument* document = m_workspace.activeDocument();
    if (!document || document->path().isEmpty()) {
        return;
    }
    QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(document->path()));
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
        item.insert(QStringLiteral("pinned"), entry.pinned);
        result.append(item);
    }
    return result;
}

void AppController::setProjectPinned(const QString& path, bool pinned)
{
    m_workspace.recentProjects().setPinned(path, pinned);

    // Written now rather than at exit. The user can see this list, and a pin
    // that vanished because the application did not close cleanly would read
    // as the feature being broken.
    if (const core::Status status = m_workspace.saveHistory(); !status) {
        qCWarning(lcUi) << "could not save recent projects:"
                        << status.error().toString();
    }
}

void AppController::forgetProject(const QString& path)
{
    m_workspace.recentProjects().remove(path);

    if (const core::Status status = m_workspace.saveHistory(); !status) {
        qCWarning(lcUi) << "could not save recent projects:"
                        << status.error().toString();
    }
}

bool AppController::pathExists(const QString& path) const
{
    return !path.isEmpty() && QFileInfo::exists(path);
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

bool AppController::openFile(const QString& path)
{
    const core::Status status = m_workspace.openFile(path);
    if (!status) {
        m_lastError = status.error().toString();
        qCWarning(lcUi) << "could not open file:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

int AppController::groupCount() const
{
    return m_workspace.editors().groupCount();
}

int AppController::activeGroup() const
{
    return m_workspace.editors().activeGroupIndex();
}

bool AppController::isSplit() const
{
    return m_workspace.editors().groupCount() > 1;
}

void AppController::closeTab(int index)
{
    m_workspace.closeTab(index);
}

void AppController::toggleSplit()
{
    // One control does both directions: the design has a single split button
    // that lights up when active, not separate split and unsplit commands.
    workspace::EditorLayout& editors = m_workspace.editors();
    if (editors.groupCount() > 1) {
        editors.closeGroup(editors.groupCount() - 1);
    } else {
        editors.split();
    }
}

void AppController::focusGroup(int index)
{
    m_workspace.editors().setActiveGroup(index);
}

bool AppController::saveFile()
{
    const core::Status status = m_workspace.saveFile();
    if (!status) {
        // A failed save must never be silent: the user believes their work is
        // on disk from this moment on.
        m_lastError = status.error().toString();
        qCWarning(lcUi) << "could not save file:" << m_lastError;
        emit errorOccurred(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

bool AppController::openFileAt(const QString& path, int line, int column)
{
    if (!openFile(path)) {
        return false;
    }

    editor::TextDocument* document = m_workspace.activeDocument();
    if (!document) {
        return false;
    }

    // Compilers count from one; the document counts from zero. A missing column
    // (0) means the start of the line.
    const editor::Position position{std::max(0, line - 1), std::max(0, column - 1)};
    document->setCursorPosition(document->buffer().clamp(position));
    return true;
}

void AppController::reportNotice(const QString& message)
{
    if (message.isEmpty()) {
        return;
    }
    m_lastError = message;
    emit errorOccurred(message);
}

bool AppController::saveFileAs(const QString& path)
{
    const core::Status status = m_workspace.saveFileAs(path);
    if (!status) {
        m_lastError = status.error().toString();
        emit errorOccurred(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

QString AppController::newFileDirectory() const
{
    // Beside the open file, so a bare name lands where the user is working.
    if (const editor::TextDocument* document = m_workspace.activeDocument();
        document && !document->path().isEmpty()) {
        return QFileInfo(document->path()).absolutePath();
    }
    return m_workspace.project().root();
}

QString AppController::resolveAgainstProject(const QString& path) const
{
    // A bare name means "here", which is beside the open file. An absolute path
    // is taken as given, so a dialog's result passes through untouched.
    if (path.isEmpty() || QFileInfo(path).isAbsolute()) {
        return path;
    }
    return QDir(newFileDirectory()).filePath(path);
}

bool AppController::createFile(const QString& path)
{
    const core::Status status = m_workspace.createFile(resolveAgainstProject(path));
    if (!status) {
        m_lastError = status.error().toString();
        emit errorOccurred(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

bool AppController::createFolder(const QString& path)
{
    const core::Status status = m_workspace.createFolder(resolveAgainstProject(path));
    if (!status) {
        m_lastError = status.error().toString();
        emit errorOccurred(m_lastError);
        return false;
    }
    m_lastError.clear();
    return true;
}

QString AppController::version()
{
    return QStringLiteral(KEYS_VERSION);
}

QString AppController::qtVersion()
{
    // The runtime version, not the compile-time one: a mismatch between them is
    // exactly the kind of thing a bug report needs to show.
    return QString::fromLatin1(qVersion());
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

    add(QStringLiteral("workbench.toggleSplit"),
        QStringLiteral("Split Editor"),
        QStringLiteral("View"),
        [this] { toggleSplit(); },
        [this] { return hasProject(); });

    add(QStringLiteral("workspace.saveFile"),
        QStringLiteral("Save"),
        QStringLiteral("File"),
        [this] { saveFile(); },
        [this] { return m_workspace.hasOpenFile(); });

    add(QStringLiteral("workspace.closeProject"),
        QStringLiteral("Close Project"),
        QStringLiteral("File"),
        [this] { closeProject(); },
        [this] { return hasProject(); });

    add(QStringLiteral("workbench.toggleProblems"),
        QStringLiteral("Toggle Problems"),
        QStringLiteral("View"),
        [this] { toggleProblems(); },
        [this] { return hasProject(); });

    add(QStringLiteral("workbench.toggleTerminal"),
        QStringLiteral("Toggle Terminal"),
        QStringLiteral("View"),
        [this] { toggleTerminal(); },
        [this] { return hasProject(); });

    add(QStringLiteral("workbench.toggleSidebar"),
        QStringLiteral("Toggle Sidebar"),
        QStringLiteral("View"),
        [this] {
            m_settings.setValue(QLatin1String(kSidebarVisibleKey), !sidebarVisible());
        });

    add(QStringLiteral("workspace.newFile"),
        QStringLiteral("New File"),
        QStringLiteral("File"),
        [this] { emit newFileRequested(); },
        [this] { return hasProject(); });

    add(QStringLiteral("workspace.newFolder"),
        QStringLiteral("New Folder"),
        QStringLiteral("File"),
        [this] { emit newFolderRequested(); },
        [this] { return hasProject(); });

    add(QStringLiteral("workspace.saveFileAs"),
        QStringLiteral("Save As"),
        QStringLiteral("File"),
        [this] { emit saveAsRequested(); },
        [this] { return m_workspace.hasOpenFile(); });

    add(QStringLiteral("workspace.openProject"),
        QStringLiteral("Open Project"),
        QStringLiteral("File"),
        [this] { emit openProjectRequested(); });

    add(QStringLiteral("workbench.openSettings"),
        QStringLiteral("Open Settings"),
        QStringLiteral("Preferences"),
        [this] { setSettingsOpen(true); });

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
