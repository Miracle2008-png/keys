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

    /// Shown in the About box, which is what somebody reporting a problem is
    /// asked for.
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)

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

    /// Whether the active group has more than one tab, and whether anything
    /// anywhere is unsaved. The menus dim against these rather than offering
    /// actions that would do nothing.
    Q_PROPERTY(int tabCount READ tabCount NOTIFY editorsChanged)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY editorsChanged)
    Q_PROPERTY(bool hasOpenFile READ hasOpenFile NOTIFY editorsChanged)

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

    [[nodiscard]] static QString version();
    [[nodiscard]] static QString qtVersion();

    [[nodiscard]] bool settingsOpen() const { return m_settingsOpen; }
    Q_INVOKABLE void setSettingsOpen(bool open);
    [[nodiscard]] QString activeView() const;
    [[nodiscard]] int sidebarWidth() const;
    [[nodiscard]] QString projectName() const;
    [[nodiscard]] QString projectRoot() const;
    [[nodiscard]] bool hasProject() const;
    [[nodiscard]] QVariantList recentProjects() const;

    [[nodiscard]] int tabCount() const;
    [[nodiscard]] bool hasUnsavedChanges() const;
    [[nodiscard]] bool hasOpenFile() const;

    /// Pins or unpins a recent project, and forgets one entirely. Both write
    /// the history immediately: this is the user editing a list they can see,
    /// and a change that survived only until a clean exit would look like the
    /// click did nothing.
    Q_INVOKABLE void setProjectPinned(const QString& path, bool pinned);
    Q_INVOKABLE void forgetProject(const QString& path);

    /// Whether a path is still there. The welcome screen dims what it cannot
    /// open rather than offering a row that fails on click.
    [[nodiscard]] Q_INVOKABLE bool pathExists(const QString& path) const;

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

    /// Writes the open document somewhere new and follows it there.
    Q_INVOKABLE bool saveFileAs(const QString& path);

    /// Creates a file or folder, relative to the project root when given a
    /// relative name - which is what a user types into a "New File" prompt.
    Q_INVOKABLE bool createFile(const QString& path);
    Q_INVOKABLE bool createFolder(const QString& path);

    /// Where a new file should default to: the directory of whatever is open,
    /// or the project root. Typing a bare name next to the file you are looking
    /// at is what people mean by "new file".
    [[nodiscard]] Q_INVOKABLE QString newFileDirectory() const;

    /// Closes a tab in the active group.
    Q_INVOKABLE void closeTab(int index);

    /// Tabs in the active group. The menu acts on whichever pane holds the
    /// caret, which is what "close this tab" means when the editor is split.
    Q_INVOKABLE void closeOtherTabs();
    Q_INVOKABLE void closeAllTabs();
    Q_INVOKABLE void nextTab();
    Q_INVOKABLE void previousTab();

    Q_INVOKABLE bool saveAllFiles();
    Q_INVOKABLE bool reloadActiveFile();

    /// The header for a source file, or the source for a header. A filename
    /// rule rather than a parse: it is right almost always, costs nothing, and
    /// is what the shortcut is for.
    Q_INVOKABLE bool switchHeaderSource();

    /// Moves the caret, opening the file first if it is not already open.
    Q_INVOKABLE void goToLine(int line);

    /// Copies the active file's path to the clipboard.
    Q_INVOKABLE void copyActivePath();

    /// Splits the editor, or collapses back to one pane if already split.
    Q_INVOKABLE void toggleSplit();

    /// Asks the window to show or hide the terminal. A signal rather than a
    /// call because the dock is a QML construct: the controller knows the
    /// command exists, the window knows what a dock is.
    Q_INVOKABLE void toggleTerminal();

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

    /// Asked for by a command; the view raises the prompt or dialog. The
    /// controller does not own UI, so it reports the intent rather than
    /// constructing a window.
    /// The terminal was asked for, from the palette, the menu or the shortcut.
    void terminalToggleRequested();

    void newFileRequested();
    void newFolderRequested();
    void saveAsRequested();
    void openProjectRequested();

private:
    /// Registers the commands the workbench itself owns. Other modules register
    /// their own; nothing here knows about them.
    void registerWorkbenchCommands();

    /// Turns a name typed into a prompt into a full path.
    [[nodiscard]] QString resolveAgainstProject(const QString& path) const;

    core::CommandRegistry& m_commands;
    config::Settings& m_settings;
    config::AnimationPolicy& m_animation;
    workspace::Workspace& m_workspace;
    QString m_lastError;
    bool m_settingsOpen = false;
};

} // namespace keys::ui
