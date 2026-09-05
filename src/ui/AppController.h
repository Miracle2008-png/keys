#pragma once

#include "config/AnimationPolicy.h"
#include "config/Settings.h"
#include "core/CommandRegistry.h"
#include "workspace/Workspace.h"

#include <QObject>
#include <QVariantList>
#include <QQmlEngine>
#include <QString>

namespace keys::ui {

/// The façade QML binds to.
///
/// QML talks to this one object rather than to core modules directly. That keeps
/// the boundary narrow — view logic cannot reach into the filesystem or spawn a
/// process on a whim — and it means the whole UI surface is reviewable in one
/// header. It also keeps QML testable: everything here is plain C++ that a test
/// can drive without a scene graph.
///
/// This is a façade, not a god object: it holds no state of its own beyond what
/// the view needs and delegates every decision to the module that owns it.
class AppController : public QObject {
    Q_OBJECT
    // Exposed to QML as "App": the QML side reads as App.sidebarVisible rather
    // than AppController.sidebarVisible, which is what the views actually mean.
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(int animationDuration READ animationDuration NOTIFY animationChanged)
    Q_PROPERTY(int fastAnimationDuration READ fastAnimationDuration NOTIFY animationChanged)

    Q_PROPERTY(bool sidebarVisible READ sidebarVisible NOTIFY workbenchChanged)
    Q_PROPERTY(QString activeView READ activeView NOTIFY workbenchChanged)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth NOTIFY workbenchChanged)

    /// Whether the settings page is showing in place of the editor. Session
    /// state rather than a setting: a user who closes Keys on the settings page
    /// wants their code back on the next launch, not the settings page.
    Q_PROPERTY(bool settingsOpen READ settingsOpen NOTIFY settingsOpenChanged)

    /// Empty until a project is opened. The UI shows the project name in the top
    /// bar and falls back to a neutral state when there is none, rather than
    /// displaying a placeholder that implies something is open.
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)
    Q_PROPERTY(QString projectRoot READ projectRoot NOTIFY projectChanged)
    Q_PROPERTY(bool hasProject READ hasProject NOTIFY projectChanged)

    /// Recent projects, newest first, as a list of {name, path, relativeTime}
    /// maps ready for a QML delegate.
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY recentProjectsChanged)

    /// How many editor panes are open, and which has focus.
    Q_PROPERTY(int groupCount READ groupCount NOTIFY editorsChanged)
    Q_PROPERTY(int activeGroup READ activeGroup NOTIFY editorsChanged)
    Q_PROPERTY(bool isSplit READ isSplit NOTIFY editorsChanged)

public:
    AppController(core::CommandRegistry& commands,
                  config::Settings& settings,
                  config::AnimationPolicy& animation,
                  workspace::Workspace& workspace,
                  QObject* parent = nullptr);

    /// Publishes the application's controller to QML. AppController takes its
    /// dependencies by reference, so the engine cannot construct one itself;
    /// main() builds the object graph and publishes the result here.
    static void setInstance(AppController* instance);

    /// QML singleton factory, returning the published instance. Ownership stays
    /// in C++.
    static AppController* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] int animationDuration() const { return m_animation.duration(); }
    [[nodiscard]] int fastAnimationDuration() const { return m_animation.fastDuration(); }

    [[nodiscard]] bool sidebarVisible() const;

    [[nodiscard]] bool settingsOpen() const { return m_settingsOpen; }
    Q_INVOKABLE void setSettingsOpen(bool open);
    [[nodiscard]] QString activeView() const;
    [[nodiscard]] int sidebarWidth() const;
    [[nodiscard]] QString projectName() const;
    [[nodiscard]] QString projectRoot() const;
    [[nodiscard]] bool hasProject() const;
    [[nodiscard]] QVariantList recentProjects() const;

    [[nodiscard]] int groupCount() const;
    [[nodiscard]] int activeGroup() const;
    [[nodiscard]] bool isSplit() const;

    /// Opens a project folder. Reports failure to the UI through lastError
    /// rather than returning silently, so a bad path is visible to the user.
    Q_INVOKABLE bool openProject(const QString& path);
    Q_INVOKABLE void closeProject();

    /// Opens a file into the editor. Reports failure through errorOccurred.
    Q_INVOKABLE bool openFile(const QString& path);

    /// Opens a file and puts the caret at a one-based line and column, which is
    /// how compilers and search results name a position. Out-of-range values are
    /// clamped by the document rather than refused: a diagnostic can name a line
    /// that a later edit removed, and landing nearby beats not opening at all.
    Q_INVOKABLE bool openFileAt(const QString& path, int line, int column);

    /// Surfaces a message from a subsystem through the same toast the workbench
    /// uses for its own errors, so a user sees one notification mechanism rather
    /// than one per feature.
    void reportNotice(const QString& message);

    /// Saves the open document. Reports failure through errorOccurred rather
    /// than losing the user's work silently.
    Q_INVOKABLE bool saveFile();

    /// Closes a tab in the active group.
    Q_INVOKABLE void closeTab(int index);

    /// Splits the editor, or collapses back to one pane if already split.
    Q_INVOKABLE void toggleSplit();

    /// Focuses a pane, so typing and commands act on it.
    Q_INVOKABLE void focusGroup(int index);

    /// The most recent failure, for the UI to surface. Cleared on the next
    /// successful operation.
    Q_INVOKABLE QString takeLastError();

    /// Runs a command by id. Returns false and logs if it is unknown or disabled,
    /// so a mis-wired shortcut is visible rather than silently dead.
    Q_INVOKABLE bool invokeCommand(const QString& id);

    /// Selects a sidebar view, or collapses the sidebar if that view is already
    /// showing — the behaviour the design specifies for the activity rail.
    Q_INVOKABLE void selectView(const QString& view);

    Q_INVOKABLE void setSidebarWidth(int width);

signals:
    void animationChanged();
    void workbenchChanged();
    void settingsOpenChanged();
    void projectChanged();
    void recentProjectsChanged();
    void editorsChanged();

    /// Emitted when an operation the user initiated fails. The UI shows this;
    /// nothing fails silently.
    void errorOccurred(const QString& message);

private:
    /// Registers the commands the workbench itself owns. Other modules register
    /// their own; nothing here knows about them.
    void registerWorkbenchCommands();

    core::CommandRegistry& m_commands;
    config::Settings& m_settings;
    config::AnimationPolicy& m_animation;
    workspace::Workspace& m_workspace;
    QString m_lastError;
    bool m_settingsOpen = false;
};

} // namespace keys::ui
