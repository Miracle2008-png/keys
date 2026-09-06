#pragma once

#include "config/Settings.h"
#include "core/Result.h"
#include "filesystem/FileWatcher.h"
#include "project/Project.h"
#include "core/TaskScheduler.h"
#include "editor/TextDocument.h"
#include "search/FileIndex.h"
#include "workspace/EditorLayout.h"
#include "workspace/FileTreeModel.h"
#include "workspace/RecentProjects.h"

#include <QObject>
#include <QString>

namespace keys::workspace {

/// What is open right now: the project, its watcher, and the session state that
/// belongs to it.
///
/// Workspace owns the lifecycle that has to happen in the correct order when a
/// project opens or closes - loading workspace settings, starting and stopping
/// file watching, recording history. Scattering that across callers is how an IDE
/// ends up watching a closed project's directories or leaking one project's
/// settings into the next.
class Workspace : public QObject {
    Q_OBJECT

public:
    Workspace(config::Settings& settings,
              core::TaskScheduler& scheduler,
              QObject* parent = nullptr);
    ~Workspace() override;

    /// Opens a project folder, in this order: close anything open, open the
    /// project, load its workspace settings layer, start watching the root,
    /// record it in history. Any step failing leaves nothing open rather than a
    /// half-opened state.
    core::Status openProject(const QString& path);

    /// Closes the current project, saving its workspace settings first.
    void closeProject();

    [[nodiscard]] bool hasProject() const { return m_project.isOpen(); }
    [[nodiscard]] const project::Project& project() const { return m_project; }
    [[nodiscard]] project::Project& project() { return m_project; }

    [[nodiscard]] fs::FileWatcher& watcher() { return m_watcher; }
    [[nodiscard]] RecentProjects& recentProjects() { return m_recentProjects; }

    /// The explorer's model. Owned here because its lifetime is the workspace's
    /// and it needs the project and watcher this class already coordinates.
    [[nodiscard]] FileTreeModel& fileTree() { return m_fileTree; }

    /// The editor panes and the tabs in them.
    [[nodiscard]] EditorLayout& editors() { return m_editors; }

    /// The project's file index, used by quick open and text search.
    [[nodiscard]] search::FileIndex& fileIndex() { return m_fileIndex; }

    /// The document being edited right now, or nullptr when nothing is open.
    [[nodiscard]] editor::TextDocument* activeDocument() const
    {
        return m_editors.activeDocument();
    }

    /// Loads a file into the active editor group. Fails if it cannot be read or
    /// is too large, leaving what was open untouched in that case.
    core::Status openFile(const QString& path);

    /// Writes the active document back to its path.
    core::Status saveFile();

    /// Writes the open document to a new path and follows it there, so the
    /// editor is now editing the new file rather than leaving the user looking
    /// at a tab whose title no longer matches what they are typing into.
    core::Status saveFileAs(const QString& path);

    /// Creates an empty file and opens it. Creating without opening would leave
    /// the user to hunt for what they just made.
    core::Status createFile(const QString& path);

    /// Writes every modified document. Reports the first failure but keeps
    /// going: one unwritable file should not silently abandon the rest.
    core::Status saveAllFiles();

    /// Re-reads the active document from disk, discarding unsaved edits.
    /// The caller confirms first - this throws work away.
    core::Status reloadActiveFile();

    core::Status createFolder(const QString& path);

    /// Closes a tab in the active group, discarding unsaved changes. The UI
    /// asks about those first.
    void closeTab(int index);

    [[nodiscard]] bool hasOpenFile() const { return m_editors.activeDocument() != nullptr; }

    /// Loads the recent-projects history. Called once at startup.
    core::Status loadHistory();
    core::Status saveHistory() const;

signals:
    void projectOpened(const QString& root);
    void projectClosed();
    void fileOpened(const QString& path);

private:
    /// Workspace settings live at <root>/.keys/settings.json and override the
    /// user layer for this project only.
    void loadWorkspaceSettings();
    void saveWorkspaceSettings() const;

    config::Settings& m_settings;
    project::Project m_project;
    fs::FileWatcher m_watcher;
    RecentProjects m_recentProjects;

    // Declared last: it holds references to the three members above, so it must
    // be destroyed before them.
    FileTreeModel m_fileTree;
    search::FileIndex m_fileIndex;
    EditorLayout m_editors;
};

} // namespace keys::workspace
