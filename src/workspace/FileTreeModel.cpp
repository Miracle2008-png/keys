#include "workspace/FileTreeModel.h"

#include "core/Log.h"
#include "core/Trace.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

using keys::core::CancellationSource;
using keys::core::CancellationToken;
using keys::fs::DirEntry;
using keys::fs::FileSystem;

namespace keys::workspace {

FileTreeModel::FileTreeModel(project::Project& project,
                             fs::FileWatcher& watcher,
                             core::TaskScheduler& scheduler,
                             QObject* parent)
    : QAbstractListModel(parent),
      m_project(project),
      m_watcher(watcher),
      m_scheduler(scheduler),
      m_cancellation(std::make_unique<CancellationSource>())
{
    connect(&m_project, &project::Project::opened, this, [this] { reload(); });
    connect(&m_project, &project::Project::closed, this, [this] { reload(); });

    // External changes reach the tree through the watcher. Only directories that
    // are actually visible are refreshed; the rest are re-read when expanded.
    connect(&m_watcher, &fs::FileWatcher::directoriesChanged, this,
            [this](const QStringList& paths) {
                for (const QString& path : paths) {
                    refreshDirectory(path);
                }
            });
}

FileTreeModel::~FileTreeModel()
{
    // In-flight reads capture `this`. Cancelling here means a worker that is
    // already running observes the flag and drops its result rather than posting
    // it back to a destroyed model.
    m_cancellation->cancel();
}

int FileTreeModel::rowCount(const QModelIndex& parent) const
{
    // A flat list has no children of any row.
    return parent.isValid() ? 0 : static_cast<int>(m_nodes.size());
}

QVariant FileTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_nodes.size()) {
        return {};
    }

    const Node& node = m_nodes.at(index.row());
    switch (role) {
    case NameRole:
        return node.name;
    case PathRole:
        return node.path;
    case DepthRole:
        return node.depth;
    case IsDirectoryRole:
        return node.isDirectory;
    case IsExpandedRole:
        return node.isExpanded;
    case HasChildrenRole:
        return node.hasChildren;
    case IsSelectedRole:
        return node.path == m_selectedPath;
    default:
        return {};
    }
}

QHash<int, QByteArray> FileTreeModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {DepthRole, "depth"},
        {IsDirectoryRole, "isDirectory"},
        {IsExpandedRole, "isExpanded"},
        {HasChildrenRole, "hasChildren"},
        {IsSelectedRole, "isSelected"},
    };
}

void FileTreeModel::reload()
{
    // Supersede any in-flight read before tearing down the rows they would
    // splice into, then start a fresh generation.
    m_cancellation->cancel();
    m_cancellation = std::make_unique<CancellationSource>();
    m_pendingLoads = 0;

    beginResetModel();
    m_nodes.clear();
    m_rowByPath.clear();
    m_pendingExpansions.clear();
    m_selectedPath.clear();
    endResetModel();

    emit countChanged();
    emit loadingChanged();

    if (!m_project.isOpen()) {
        return;
    }

    // -1 means "splice at the top level": the root's own children are depth 0
    // and the project root itself is not drawn as a row.
    loadChildren(m_project.root(), -1);
}

void FileTreeModel::loadChildren(const QString& path, int parentRow)
{
    const bool atRoot = parentRow < 0;
    const int depth = atRoot ? 0 : m_nodes.at(parentRow).depth + 1;

    // The parent is captured by *path*, not by row. Rows shift whenever another
    // directory finishes loading, so a captured index can point at a different
    // node by the time this read completes - which would splice a folder's
    // children under an unrelated one.
    const QString parentPath = atRoot ? QString() : m_nodes.at(parentRow).path;

    const CancellationToken token = m_cancellation->token();

    ++m_pendingLoads;
    emit loadingChanged();

    // The read runs on the pool; the splice happens back on this object's
    // thread. postWithResult drops the completion if the model is destroyed
    // first, which is what makes capturing `this` safe here.
    m_scheduler.postWithResult<QList<DirEntry>>(
        this,
        [path, token] {
            const core::Result<QList<DirEntry>> entries =
                FileSystem::listDirectory(path, token);
            return entries.hasValue() ? entries.value() : QList<DirEntry>{};
        },
        [this, parentPath, atRoot, depth, token](QList<DirEntry> entries) {
            --m_pendingLoads;
            emit loadingChanged();

            // A reload happened while this was in flight; its rows belong to a
            // tree that no longer exists.
            if (token.isCancelled()) {
                return;
            }

            // Filter here rather than in the worker so the ignore rules are read
            // on one thread only.
            QList<DirEntry> visible;
            visible.reserve(entries.size());
            for (const DirEntry& entry : entries) {
                const QString relative = m_project.relativePath(entry.path);
                if (relative.isEmpty()
                    || !m_project.ignoreRules().isIgnored(relative, entry.isDirectory)) {
                    visible.append(entry);
                }
            }

            // The parent may have been collapsed or removed while the read ran,
            // so resolve its row from the path rather than trusting an index.
            int insertAt = 0;
            if (!atRoot) {
                const int row = indexOfPath(parentPath);
                if (row < 0 || !m_nodes.at(row).isExpanded) {
                    return;
                }
                insertAt = row + 1;

                // Record whether the directory turned out to be empty, so the
                // chevron can be dropped.
                if (m_nodes.at(row).hasChildren != !visible.isEmpty()) {
                    m_nodes[row].hasChildren = !visible.isEmpty();
                    const QModelIndex changed = index(row, 0);
                    emit dataChanged(changed, changed, {HasChildrenRole});
                }
            }

            if (visible.isEmpty()) {
                return;
            }

            QList<Node> rows;
            rows.reserve(visible.size());
            for (const DirEntry& entry : visible) {
                Node node;
                node.name = entry.name;
                node.path = entry.path;
                node.depth = depth;
                node.isDirectory = entry.isDirectory;
                node.isExpanded = false;
                node.hasChildren = entry.isDirectory;
                rows.append(std::move(node));
            }

            beginInsertRows({}, insertAt, insertAt + static_cast<int>(rows.size()) - 1);
            for (int i = 0; i < rows.size(); ++i) {
                m_nodes.insert(insertAt + i, rows.at(i));
            }
            endInsertRows();

            // Rows below the splice point have shifted, so every row index is
            // potentially stale.
            reindex();
            emit countChanged();

            // Re-expand anything a refresh had open before its rows were
            // removed. Done after the splice, because the rows must exist first.
            if (!m_pendingExpansions.isEmpty()) {
                for (const Node& row : rows) {
                    if (row.isDirectory && m_pendingExpansions.contains(row.path)) {
                        m_pendingExpansions.remove(row.path);
                        toggleExpanded(indexOfPath(row.path));
                    }
                }
            }
        },
        core::TaskScheduler::Priority::Interactive);
}

void FileTreeModel::toggleExpanded(int row)
{
    if (row < 0 || row >= m_nodes.size()) {
        return;
    }

    Node& node = m_nodes[row];
    if (!node.isDirectory) {
        return;
    }

    if (node.isExpanded) {
        node.isExpanded = false;
        collapseSubtree(row);

        // Stop watching a directory the user can no longer see. Watch handles
        // are a finite platform resource, and an IDE that watches every folder
        // ever opened exhausts them on a large repository.
        m_watcher.unwatchDirectory(node.path);
    } else {
        node.isExpanded = true;

        // Watch before reading, so a change arriving during the read is not lost.
        m_watcher.watchDirectory(node.path);
        loadChildren(node.path, row);
    }

    const QModelIndex changed = index(row, 0);
    emit dataChanged(changed, changed, {IsExpandedRole});
}

int FileTreeModel::collapseSubtree(int row)
{
    if (row < 0 || row >= m_nodes.size()) {
        return 0;
    }

    const int depth = m_nodes.at(row).depth;

    // Everything after `row` that is deeper than it belongs to its subtree; the
    // first row at the same or shallower depth ends it.
    int last = row;
    while (last + 1 < m_nodes.size() && m_nodes.at(last + 1).depth > depth) {
        ++last;
    }

    const int removed = last - row;
    if (removed == 0) {
        return 0;
    }

    beginRemoveRows({}, row + 1, last);
    m_nodes.remove(row + 1, removed);
    endRemoveRows();

    reindex();
    emit countChanged();
    return removed;
}

void FileTreeModel::activate(int row)
{
    if (row < 0 || row >= m_nodes.size()) {
        return;
    }

    const Node& node = m_nodes.at(row);
    if (node.isDirectory) {
        toggleExpanded(row);
        return;
    }

    setSelectedPath(node.path);
    emit fileActivated(node.path);
}

void FileTreeModel::setSelectedPath(const QString& path)
{
    if (path == m_selectedPath) {
        return;
    }

    const int previous = indexOfPath(m_selectedPath);
    m_selectedPath = path;
    const int current = indexOfPath(m_selectedPath);

    // Repaint only the two rows whose selection actually changed, rather than
    // invalidating the whole view.
    if (previous >= 0) {
        const QModelIndex changed = index(previous, 0);
        emit dataChanged(changed, changed, {IsSelectedRole});
    }
    if (current >= 0) {
        const QModelIndex changed = index(current, 0);
        emit dataChanged(changed, changed, {IsSelectedRole});
    }
}

void FileTreeModel::refreshDirectory(const QString& path)
{
    const QString normalized = FileSystem::normalize(path);

    // A refresh removes rows and reloads them asynchronously, so which folders
    // were open has to be remembered across the gap - otherwise saving a file
    // would silently collapse the user's whole tree.
    const auto rememberExpanded = [this](const QString& under) {
        for (const Node& node : m_nodes) {
            if (node.isExpanded && node.path.startsWith(under)) {
                m_pendingExpansions.insert(node.path);
            }
        }
    };

    // The project root is refreshed by reloading its top level; it has no row.
    if (normalized == m_project.root()) {
        rememberExpanded(normalized);

        beginResetModel();
        m_nodes.clear();
        m_rowByPath.clear();
        endResetModel();

        loadChildren(normalized, -1);
        return;
    }

    const int row = indexOfPath(normalized);
    if (row < 0 || !m_nodes.at(row).isExpanded) {
        // Not visible, or collapsed: the change is picked up when it is next
        // expanded. Refreshing it now would be work nobody can see.
        return;
    }

    rememberExpanded(normalized + QLatin1Char('/'));

    collapseSubtree(row);
    loadChildren(normalized, row);
}

int FileTreeModel::indexOfPath(const QString& path) const
{
    if (path.isEmpty()) {
        return -1;
    }
    return m_rowByPath.value(path, -1);
}

void FileTreeModel::reindex()
{
    m_rowByPath.clear();
    m_rowByPath.reserve(m_nodes.size());
    for (int i = 0; i < m_nodes.size(); ++i) {
        m_rowByPath.insert(m_nodes.at(i).path, i);
    }
}


// ---- File operations --------------------------------------------------------

namespace {

/// Names that cannot be created on the target filesystem, or that would be
/// confusing enough to refuse. Checked before touching the disk so the user gets
/// a clear message instead of a platform error code.
///
/// The reserved-device names are Windows-specific but rejected everywhere: a
/// project shared across platforms should not contain a file that one of them
/// cannot check out.
bool isValidEntryName(const QString& name, QString& reason)
{
    if (name.isEmpty()) {
        reason = QCoreApplication::translate("FileTreeModel", "The name cannot be empty.");
        return false;
    }
    if (name == QLatin1String(".") || name == QLatin1String("..")) {
        reason = QCoreApplication::translate("FileTreeModel",
                                             "\"%1\" is not a usable name.").arg(name);
        return false;
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))) {
        reason = QCoreApplication::translate(
            "FileTreeModel", "A name cannot contain a path separator.");
        return false;
    }

    static const QString invalid = QStringLiteral(":*?\"<>|");
    for (const QChar character : name) {
        if (invalid.contains(character)) {
            reason = QCoreApplication::translate(
                         "FileTreeModel", "A name cannot contain %1.").arg(character);
            return false;
        }
    }

    static const QStringList reserved = {
        QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"),
        QStringLiteral("NUL"), QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"), QStringLiteral("LPT1"),
        QStringLiteral("LPT2"), QStringLiteral("LPT3"),
    };
    const QString base = name.section(QLatin1Char('.'), 0, 0).toUpper();
    if (reserved.contains(base)) {
        reason = QCoreApplication::translate(
                     "FileTreeModel", "\"%1\" is a reserved name on Windows.").arg(base);
        return false;
    }

    return true;
}

} // namespace

bool FileTreeModel::createFile(const QString& directoryPath, const QString& name)
{
    QString reason;
    if (!isValidEntryName(name, reason)) {
        emit errorOccurred(reason);
        return false;
    }

    const QString directory = FileSystem::normalize(directoryPath);
    if (!m_project.contains(directory)) {
        // The explorer must not be able to write outside the open project,
        // whatever path reaches this call.
        emit errorOccurred(tr("That folder is outside the open project."));
        return false;
    }

    const QString path = FileSystem::normalize(QDir(directory).filePath(name));
    if (const core::Status status = FileSystem::createFile(path); !status) {
        emit errorOccurred(status.error().message());
        return false;
    }

    // Refresh rather than inserting a row directly: the new entry has to land in
    // the same sort position the next listing would give it, and re-reading is
    // the only way to be sure of that.
    refreshDirectory(directory);
    setSelectedPath(path);
    return true;
}

bool FileTreeModel::createFolder(const QString& directoryPath, const QString& name)
{
    QString reason;
    if (!isValidEntryName(name, reason)) {
        emit errorOccurred(reason);
        return false;
    }

    const QString directory = FileSystem::normalize(directoryPath);
    if (!m_project.contains(directory)) {
        emit errorOccurred(tr("That folder is outside the open project."));
        return false;
    }

    const QString path = FileSystem::normalize(QDir(directory).filePath(name));
    if (const core::Status status = FileSystem::createDirectory(path); !status) {
        emit errorOccurred(status.error().message());
        return false;
    }

    refreshDirectory(directory);
    setSelectedPath(path);
    return true;
}

bool FileTreeModel::rename(const QString& path, const QString& newName)
{
    QString reason;
    if (!isValidEntryName(newName, reason)) {
        emit errorOccurred(reason);
        return false;
    }

    const QString from = FileSystem::normalize(path);
    if (!m_project.contains(from) || from == m_project.root()) {
        // Renaming the project root would leave the workspace pointing at a path
        // that no longer exists.
        emit errorOccurred(tr("That item cannot be renamed."));
        return false;
    }

    const QString directory = QFileInfo(from).absolutePath();
    const QString to = FileSystem::normalize(QDir(directory).filePath(newName));
    if (from == to) {
        return true;
    }

    if (const core::Status status = FileSystem::rename(from, to); !status) {
        emit errorOccurred(status.error().message());
        return false;
    }

    refreshDirectory(FileSystem::normalize(directory));
    setSelectedPath(to);
    return true;
}

bool FileTreeModel::moveToTrash(const QString& path)
{
    const QString target = FileSystem::normalize(path);
    if (!m_project.contains(target) || target == m_project.root()) {
        emit errorOccurred(tr("That item cannot be deleted."));
        return false;
    }

    if (const core::Status status = FileSystem::moveToTrash(target); !status) {
        emit errorOccurred(status.error().message());
        return false;
    }

    if (m_selectedPath == target || m_selectedPath.startsWith(target + QLatin1Char('/'))) {
        setSelectedPath(QString());
    }

    refreshDirectory(FileSystem::normalize(QFileInfo(target).absolutePath()));
    return true;
}

} // namespace keys::workspace
