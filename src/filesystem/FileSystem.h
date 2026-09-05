#pragma once

#include "core/Cancellation.h"
#include "core/Result.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace keys::fs {

/// What a directory entry is. Kept minimal: the explorer needs to know whether to
/// draw a chevron and how to sort, not the full stat block.
struct DirEntry {
    QString name;         ///< file name only, not the full path
    QString path;         ///< absolute, normalised to forward slashes
    bool isDirectory = false;
    bool isSymLink = false;
    qint64 size = 0;
    QDateTime modified;

    /// Directories before files, then case-insensitive by name. This is the order
    /// the explorer displays, applied at the source so every consumer agrees.
    [[nodiscard]] bool operator<(const DirEntry& other) const;
};

/// Synchronous filesystem primitives.
///
/// These block, so they must not be called from the UI thread — callers go through
/// AsyncFileSystem, which runs them on the worker pool. They are exposed directly
/// because worker code and tests both need them without a scheduler round trip.
///
/// Paths are normalised to forward slashes throughout Keys, including on Windows,
/// so a path used as a map key or compared for equality behaves consistently.
class FileSystem {
public:
    /// Normalises separators and removes redundant elements. Does not touch the
    /// disk, so it is safe to call from anywhere.
    [[nodiscard]] static QString normalize(const QString& path);

    [[nodiscard]] static bool exists(const QString& path);
    [[nodiscard]] static bool isDirectory(const QString& path);

    /// Reads a whole file as UTF-8.
    ///
    /// Refuses files above `maxBytes`: the editor cannot usefully open a
    /// multi-gigabyte file, and attempting it would exhaust memory rather than
    /// fail cleanly. Callers get NotSupported and can offer their own handling.
    [[nodiscard]] static core::Result<QString> readTextFile(
        const QString& path, qint64 maxBytes = kDefaultMaxFileSize);

    /// Writes UTF-8 atomically: content goes to a temporary file which is renamed
    /// over the target on commit, so an interrupted save cannot truncate the
    /// user's file.
    [[nodiscard]] static core::Status writeTextFile(const QString& path,
                                                    const QString& content);

    /// Lists one directory. Not recursive - the explorer expands lazily, and
    /// walking a deep tree eagerly is the main way a file browser becomes slow.
    [[nodiscard]] static core::Result<QList<DirEntry>> listDirectory(
        const QString& path, const core::CancellationToken& token = {});

    [[nodiscard]] static core::Status createDirectory(const QString& path);
    [[nodiscard]] static core::Status createFile(const QString& path);

    /// Moves to the system trash where the platform supports it, so a mistaken
    /// delete is recoverable. Falls back to permanent removal only when the
    /// platform has no trash, and says so in the error if that fails.
    [[nodiscard]] static core::Status moveToTrash(const QString& path);

    [[nodiscard]] static core::Status rename(const QString& from, const QString& to);

    /// Files larger than this are refused by readTextFile. 64 MiB is far above any
    /// source file and well below the point where reading becomes hostile.
    static constexpr qint64 kDefaultMaxFileSize = 64 * 1024 * 1024;
};

} // namespace keys::fs
