#include "workspace/Workspace.h"

#include "config/SettingsStore.h"
#include "core/Log.h"
#include "core/Trace.h"
#include "filesystem/FileSystem.h"

using keys::core::Ok;
using keys::core::Status;

namespace keys::workspace {

Workspace::Workspace(config::Settings& settings,
                     core::TaskScheduler& scheduler,
                     QObject* parent)
    : QObject(parent),
      m_settings(settings),
      m_fileTree(m_project, m_watcher, scheduler, this)
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

    // The open document belongs to the project that is closing.
    m_document.setText(QString());
    m_document.setPath(QString());

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

Status Workspace::openFile(const QString& path)
{
    KEYS_TRACE("Workspace::openFile");

    const QString normalized = fs::FileSystem::normalize(path);

    // Read first, and only touch the document if it succeeds: a failed open
    // must leave whatever the user was editing exactly as it was.
    const core::Result<QString> contents = fs::FileSystem::readTextFile(normalized);
    if (!contents) {
        return contents.error();
    }

    // The previously open file stops being watched, so a background change to a
    // file nobody is looking at costs nothing.
    if (!m_document.path().isEmpty()) {
        m_watcher.unwatchFile(m_document.path());
    }

    m_document.setText(contents.value());
    m_document.setPath(normalized);

    // Watch the open file so an external edit - a rebase, a formatter - can be
    // reported rather than silently overwritten on the next save.
    m_watcher.watchFile(normalized);

    emit fileOpened(normalized);
    return Ok();
}

Status Workspace::saveFile()
{
    if (m_document.path().isEmpty()) {
        return core::Err(core::ErrorCode::InvalidArgument,
                         QStringLiteral("This document has no file to save to"));
    }

    // Suppress the watcher around our own write, or saving would come straight
    // back as an "external change" notification.
    m_watcher.suppress(m_document.path());

    const Status status =
        fs::FileSystem::writeTextFile(m_document.path(), m_document.text());

    m_watcher.unsuppress(m_document.path());

    if (!status) {
        return status;
    }

    m_document.markSaved();
    return Ok();
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
