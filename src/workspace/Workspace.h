#pragma once

#include "config/Settings.h"
#include "core/Result.h"
#include "filesystem/FileWatcher.h"
#include "project/Project.h"
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
    Workspace(config::Settings& settings, QObject* parent = nullptr);
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

    /// Loads the recent-projects history. Called once at startup.
    core::Status loadHistory();
    core::Status saveHistory() const;

signals:
    void projectOpened(const QString& root);
    void projectClosed();

private:
    /// Workspace settings live at <root>/.keys/settings.json and override the
    /// user layer for this project only.
    void loadWorkspaceSettings();
    void saveWorkspaceSettings() const;

    config::Settings& m_settings;
    project::Project m_project;
    fs::FileWatcher m_watcher;
    RecentProjects m_recentProjects;
};

} // namespace keys::workspace
