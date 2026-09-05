#pragma once

#include "core/Cancellation.h"
#include "core/TaskScheduler.h"
#include "filesystem/FileSystem.h"
#include "filesystem/FileWatcher.h"
#include "project/Project.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QString>

#include <memory>

namespace keys::workspace {

/// The file explorer's model.
///
/// **Why a flat list model, not QAbstractItemModel.**
/// The explorer renders as a flat list of visible rows: a collapsed folder's
/// children simply are not rows. Exposing that shape directly means QML's
/// ListView can recycle delegates and only ever instantiate what is on screen,
/// which is what keeps a 50,000-file project scrolling at frame rate. A tree
/// model would force QML into a TreeView whose row mapping we would then have to
/// flatten anyway, at the cost of a far more intricate index scheme.
///
/// **Laziness.** A directory's children are read the first time it is expanded
/// and then cached. Walking a whole project up front is the single most common
/// reason a file browser feels slow to open, and it wastes work on the vast
/// majority of directories the user never looks at.
///
/// **Off the UI thread.** Every directory read runs on the task scheduler. A
/// directory on a slow network share, or one with 100,000 entries, must not
/// freeze the window.
class FileTreeModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        DepthRole,       ///< 0 for a root child; drives the row's indent
        IsDirectoryRole,
        IsExpandedRole,
        HasChildrenRole, ///< false for an expanded, genuinely empty directory
        IsSelectedRole,
    };

    FileTreeModel(project::Project& project,
                  fs::FileWatcher& watcher,
                  core::TaskScheduler& scheduler,
                  QObject* parent = nullptr);
    ~FileTreeModel() override;

    // QAbstractListModel
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool isLoading() const { return m_pendingLoads > 0; }

    /// Expands or collapses the directory at `row`. Loads its children on first
    /// expansion; subsequent expansions reuse the cache.
    Q_INVOKABLE void toggleExpanded(int row);

    /// Marks a row as the selected one. Purely presentational — opening a file
    /// is the workspace's job, signalled through fileActivated.
    Q_INVOKABLE void setSelectedPath(const QString& path);
    [[nodiscard]] QString selectedPath() const { return m_selectedPath; }

    /// Called when a row is activated (double click, or Enter). Emits
    /// fileActivated for a file; toggles a directory.
    Q_INVOKABLE void activate(int row);

    /// Re-reads a directory that changed on disk, preserving which of its
    /// descendants are expanded. Called by the watcher and by the file
    /// operations below.
    void refreshDirectory(const QString& path);

    // ---- File operations --------------------------------------------------
    //
    // Each validates against the project root before touching the disk: a path
    // from outside the project must never be created, renamed or deleted by the
    // explorer, whatever the caller passes in.
    //
    // Failures are reported through errorOccurred rather than swallowed, so a
    // name collision or a permissions problem is visible to the user.

    /// Creates an empty file inside `directoryPath` and selects it.
    Q_INVOKABLE bool createFile(const QString& directoryPath, const QString& name);

    /// Creates a folder inside `directoryPath`.
    Q_INVOKABLE bool createFolder(const QString& directoryPath, const QString& name);

    /// Renames a file or folder in place. `newName` is a name, not a path;
    /// anything containing a separator is rejected so a rename cannot move an
    /// item somewhere else by accident.
    Q_INVOKABLE bool rename(const QString& path, const QString& newName);

    /// Moves a file or folder to the system trash, so a mistake is recoverable.
    Q_INVOKABLE bool moveToTrash(const QString& path);

    /// Discards everything and reloads the project root. Called when the project
    /// changes.
    void reload();

signals:
    void loadingChanged();
    void countChanged();

    /// A file the user chose to open. The model does not open files itself;
    /// that belongs to the workspace.
    void fileActivated(const QString& path);

    /// An operation the user started failed, with a message fit to show them.
    void errorOccurred(const QString& message);

private:
    /// One row in the flattened, currently visible tree.
    struct Node {
        QString name;
        QString path;
        int depth = 0;
        bool isDirectory = false;
        bool isExpanded = false;

        /// Unknown until the directory is read. Until then a directory is drawn
        /// with a chevron, because assuming otherwise would make every folder
        /// look empty until touched.
        bool hasChildren = true;
    };

    /// Reads `path` off the UI thread and splices the result in below `parentRow`.
    void loadChildren(const QString& path, int parentRow);

    /// Removes the rows belonging to `row`'s subtree. Returns how many went.
    int collapseSubtree(int row);

    [[nodiscard]] int indexOfPath(const QString& path) const;

    /// Rebuilds m_rowByPath from m_nodes. Called after any structural change.
    void reindex();

    project::Project& m_project;
    fs::FileWatcher& m_watcher;
    core::TaskScheduler& m_scheduler;

    QList<Node> m_nodes;

    /// path -> row, so lookups are constant time.
    ///
    /// The explorer resolves a path to a row on every selection change, every
    /// watcher event and every completed directory read. A linear scan is fine
    /// at a hundred rows and is a visible stall at fifty thousand, which is
    /// exactly the project size this model is built for.
    ///
    /// Kept in step by reindex() after each structural change. Rebuilding the
    /// whole map is O(n) but happens once per change, where the scans it
    /// replaces were O(n) *per lookup*.
    QHash<QString, int> m_rowByPath;

    /// Directories to expand once their rows exist. A refresh removes rows and
    /// reloads them asynchronously, so expansion state cannot be restored
    /// synchronously; this records the intent and loadChildren applies it.
    QSet<QString> m_pendingExpansions;

    QString m_selectedPath;

    /// Cancels in-flight directory reads when the model is reset, so a slow read
    /// for a closed project cannot splice rows into the next one.
    std::unique_ptr<core::CancellationSource> m_cancellation;

    int m_pendingLoads = 0;
};

} // namespace keys::workspace
