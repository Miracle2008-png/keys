#include "filesystem/FileWatcher.h"

#include "core/Log.h"
#include "filesystem/FileSystem.h"

namespace keys::fs {

FileWatcher::FileWatcher(QObject* parent) : QObject(parent)
{
    m_coalesceTimer.setSingleShot(true);
    m_coalesceTimer.setInterval(kCoalesceIntervalMs);
    connect(&m_coalesceTimer, &QTimer::timeout, this, &FileWatcher::flush);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FileWatcher::onDirectoryEvent);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &FileWatcher::onFileEvent);
}

void FileWatcher::watchDirectory(const QString& path)
{
    const QString normalized = FileSystem::normalize(path);
    if (normalized.isEmpty() || m_directories.contains(normalized)) {
        return;
    }

    if (!m_watcher.addPath(normalized)) {
        // Almost always the platform's watch limit. Log rather than fail: a
        // missing watch degrades freshness, it does not break the explorer, and
        // the user should not see an error for a directory they merely expanded.
        qCWarning(lcFs) << "could not watch directory" << normalized;
        return;
    }
    m_directories.insert(normalized);
}

void FileWatcher::unwatchDirectory(const QString& path)
{
    const QString normalized = FileSystem::normalize(path);
    if (m_directories.remove(normalized)) {
        m_watcher.removePath(normalized);
    }
}

void FileWatcher::watchFile(const QString& path)
{
    const QString normalized = FileSystem::normalize(path);
    if (normalized.isEmpty() || m_files.contains(normalized)) {
        return;
    }

    if (!m_watcher.addPath(normalized)) {
        qCWarning(lcFs) << "could not watch file" << normalized;
        return;
    }
    m_files.insert(normalized);
}

void FileWatcher::unwatchFile(const QString& path)
{
    const QString normalized = FileSystem::normalize(path);
    if (m_files.remove(normalized)) {
        m_watcher.removePath(normalized);
    }
}

void FileWatcher::clear()
{
    const QStringList directories = m_watcher.directories();
    const QStringList files = m_watcher.files();
    if (!directories.isEmpty()) {
        m_watcher.removePaths(directories);
    }
    if (!files.isEmpty()) {
        m_watcher.removePaths(files);
    }

    m_directories.clear();
    m_files.clear();
    m_pendingDirectories.clear();
    m_pendingFiles.clear();
    m_suppressed.clear();
    m_coalesceTimer.stop();
}

void FileWatcher::suppress(const QString& path)
{
    m_suppressed.insert(FileSystem::normalize(path));
}

void FileWatcher::unsuppress(const QString& path)
{
    m_suppressed.remove(FileSystem::normalize(path));
}

int FileWatcher::watchedDirectoryCount() const
{
    return static_cast<int>(m_directories.size());
}

int FileWatcher::watchedFileCount() const
{
    return static_cast<int>(m_files.size());
}

void FileWatcher::onDirectoryEvent(const QString& path)
{
    const QString normalized = FileSystem::normalize(path);
    if (m_suppressed.contains(normalized)) {
        return;
    }

    m_pendingDirectories.insert(normalized);

    // Restarting rather than starting: a burst of events keeps pushing the flush
    // out, so one batch is emitted after the burst settles instead of one per
    // event. This is what turns a build's thousand writes into a single refresh.
    m_coalesceTimer.start();
}

void FileWatcher::onFileEvent(const QString& path)
{
    const QString normalized = FileSystem::normalize(path);
    if (m_suppressed.contains(normalized)) {
        return;
    }

    m_pendingFiles.insert(normalized);
    m_coalesceTimer.start();
}

void FileWatcher::flush()
{
    // An atomic save replaces the file, which makes the inode Qt was watching
    // disappear and silently drops the watch. Re-add anything still in the
    // intended watch list but no longer registered, or the second external edit
    // to a file would go unnoticed.
    const QSet<QString> registeredFiles(m_watcher.files().cbegin(), m_watcher.files().cend());
    for (const QString& file : m_files) {
        if (!registeredFiles.contains(file) && FileSystem::exists(file)) {
            m_watcher.addPath(file);
        }
    }

    const QSet<QString> registeredDirs(m_watcher.directories().cbegin(),
                                       m_watcher.directories().cend());
    for (const QString& directory : m_directories) {
        if (!registeredDirs.contains(directory) && FileSystem::exists(directory)) {
            m_watcher.addPath(directory);
        }
    }

    if (!m_pendingDirectories.isEmpty()) {
        QStringList paths(m_pendingDirectories.cbegin(), m_pendingDirectories.cend());
        m_pendingDirectories.clear();
        qCDebug(lcFs) << "directories changed:" << paths.size();
        emit directoriesChanged(paths);
    }

    if (!m_pendingFiles.isEmpty()) {
        QStringList paths(m_pendingFiles.cbegin(), m_pendingFiles.cend());
        m_pendingFiles.clear();
        qCDebug(lcFs) << "files changed:" << paths.size();
        emit filesChanged(paths);
    }
}

} // namespace keys::fs
