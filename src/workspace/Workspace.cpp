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
      m_fileTree(m_project, m_watcher, scheduler, this),
      m_fileIndex(m_project, scheduler, this)
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

    // Open documents belong to the project that is closing.
    m_editors.reset();

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

    if (!m_editors.openInActiveGroup(normalized, contents.value())) {
        return core::Err(core::ErrorCode::Unknown,
                         QStringLiteral("No editor group is available"), normalized);
    }

    // Watch the open file so an external edit - a rebase, a formatter - can be
    // reported rather than silently overwritten on the next save. Files stay
    // watched while a tab holds them; closing the tab releases the watch.
    m_watcher.watchFile(normalized);

    emit fileOpened(normalized);
    return Ok();
}

Status Workspace::saveFile()
{
    editor::TextDocument* document = m_editors.activeDocument();
    if (!document || document->path().isEmpty()) {
        return core::Err(core::ErrorCode::InvalidArgument,
                         QStringLiteral("There is nothing to save"));
    }

    // Suppress the watcher around our own write, or saving would come straight
    // back as an "external change" notification.
    m_watcher.suppress(document->path());

    const Status status =
        fs::FileSystem::writeTextFile(document->path(), document->text());

    m_watcher.unsuppress(document->path());

    if (!status) {
        return status;
    }

    document->markSaved();
    return Ok();
}

void Workspace::closeTab(int index)
{
    EditorGroup* group = m_editors.activeGroup();
    if (!group) {
        return;
    }

    // Release the watch for a file no tab holds any more. Checking every group
    // matters because the same file can be open in a split.
    const editor::TextDocument* closing = group->documentAt(index);
    const QString path = closing ? closing->path() : QString();

    group->closeTab(index);

    if (!path.isEmpty()) {
        bool stillOpen = false;
        for (int i = 0; i < m_editors.groupCount() && !stillOpen; ++i) {
            stillOpen = m_editors.groupAt(i)->indexOfPath(path) >= 0;
        }
        if (!stillOpen) {
            m_watcher.unwatchFile(path);
        }
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
