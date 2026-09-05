#include "workspace/Workspace.h"

#include "config/SettingsStore.h"
#include "core/Log.h"
#include "core/Trace.h"

using keys::core::Ok;
using keys::core::Status;

namespace keys::workspace {

Workspace::Workspace(config::Settings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
}

Workspace::~Workspace()
{
    // Stop watching before the watcher's dependencies go away. Not calling
    // closeProject() here: emitting signals during destruction would reach
    // half-destroyed observers.
    m_watcher.clear();
}

Status Workspace::openProject(const QString& path)
{
    KEYS_TRACE("Workspace::openProject");

    // Close first. Opening over an open project would leave the old project's
    // watches running and its settings layer active.
    if (m_project.isOpen()) {
        closeProject();
    }

    if (const Status status = m_project.open(path); !status) {
        return status;
    }

    loadWorkspaceSettings();

    // Watch the root so top-level additions and deletions appear without the user
    // refreshing. Subdirectories are watched as the explorer expands them, which
    // is what keeps the watch count bounded on a large repository.
    m_watcher.watchDirectory(m_project.root());

    m_recentProjects.record(m_project.root());

    emit projectOpened(m_project.root());
    return Ok();
}

void Workspace::closeProject()
{
    if (!m_project.isOpen()) {
        return;
    }

    saveWorkspaceSettings();

    // Release every watch before the project is torn down, so nothing keeps
    // reporting changes for a project that is no longer open.
    m_watcher.clear();

    // Drop the workspace layer so this project's overrides cannot leak into the
    // next one opened.
    m_settings.clearWorkspaceLayer();

    m_project.close();

    emit projectClosed();
}

void Workspace::loadWorkspaceSettings()
{
    const QString path = config::SettingsStore::workspaceSettingsPath(m_project.root());
    if (const Status status = config::SettingsStore::load(
            m_settings, config::Settings::Layer::Workspace, path);
        !status) {
        // A malformed workspace settings file must not prevent the project from
        // opening; the user needs the editor to fix it in.
        qCWarning(lcConfig) << "could not load workspace settings:"
                            << status.error().toString();
    }
}

void Workspace::saveWorkspaceSettings() const
{
    // Only write if the project actually has overrides. Creating a .keys folder
    // in every project the user merely opens would litter their repositories -
    // and show up in their git status.
    if (m_settings.layerValues(config::Settings::Layer::Workspace).isEmpty()) {
        return;
    }

    const QString path = config::SettingsStore::workspaceSettingsPath(m_project.root());
    if (const Status status = config::SettingsStore::save(
            m_settings, config::Settings::Layer::Workspace, path);
        !status) {
        qCWarning(lcConfig) << "could not save workspace settings:"
                            << status.error().toString();
    }
}

Status Workspace::loadHistory()
{
    return m_recentProjects.load(RecentProjects::defaultPath());
}

Status Workspace::saveHistory() const
{
    return m_recentProjects.save(RecentProjects::defaultPath());
}

} // namespace keys::workspace
