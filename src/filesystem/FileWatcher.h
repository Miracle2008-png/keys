#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

namespace keys::fs {

/// Watches paths for external modification.
///
/// Two problems make a naive QFileSystemWatcher unusable in an IDE, and this class
/// exists to solve both:
///
/// 1. **Storms.** A build, a branch switch or a formatter touches hundreds of
///    files in milliseconds. Reacting per event would rebuild the explorer
///    hundreds of times. Events are coalesced over a short window and emitted
///    once as a batch.
///
/// 2. **Watch exhaustion.** Every platform caps how many paths can be watched,
///    and a large repository exceeds it easily. Only directories that are
///    actually expanded in the explorer, plus files that are open in an editor,
///    are watched - never the whole tree.
///
/// Editors also need to distinguish a change they made from one made outside the
/// application. suppress() covers a path for the duration of a save so Keys does
/// not report its own writes back to itself as external changes.
class FileWatcher : public QObject {
    Q_OBJECT

public:
    explicit FileWatcher(QObject* parent = nullptr);

    /// Starts watching a directory. Watching the same path twice is harmless.
    void watchDirectory(const QString& path);
    void unwatchDirectory(const QString& path);

    /// Starts watching a single file, for open editors detecting external edits.
    void watchFile(const QString& path);
    void unwatchFile(const QString& path);

    /// Stops watching everything. Called when a project closes.
    void clear();

    /// Ignores events for `path` until unsuppress() is called. Used around the
    /// application's own writes so a save is not reported as an external change.
    void suppress(const QString& path);
    void unsuppress(const QString& path);

    [[nodiscard]] int watchedDirectoryCount() const;
    [[nodiscard]] int watchedFileCount() const;

    /// How long events are coalesced before being emitted. Long enough to absorb
    /// a build's write burst, short enough that the explorer does not feel stale.
    static constexpr int kCoalesceIntervalMs = 120;

signals:
    /// Emitted once per coalescing window with every directory whose contents
    /// changed. Consumers re-list only these directories.
    void directoriesChanged(const QStringList& paths);

    /// Emitted once per coalescing window with every watched file that changed
    /// on disk outside Keys.
    void filesChanged(const QStringList& paths);

private:
    void onDirectoryEvent(const QString& path);
    void onFileEvent(const QString& path);
    void flush();

    QFileSystemWatcher m_watcher;
    QTimer m_coalesceTimer;

    QSet<QString> m_pendingDirectories;
    QSet<QString> m_pendingFiles;
    QSet<QString> m_suppressed;

    /// Qt drops a watch when the path disappears, which happens routinely during
    /// atomic saves (write temp, rename over target). These sets are the intended
    /// watch list so a dropped watch can be re-established.
    QSet<QString> m_directories;
    QSet<QString> m_files;
};

} // namespace keys::fs
