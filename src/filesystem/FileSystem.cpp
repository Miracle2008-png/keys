#include "filesystem/FileSystem.h"

#include "core/Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

using keys::core::ErrorCode;
using keys::core::Err;
using keys::core::Ok;
using keys::core::Result;
using keys::core::Status;

namespace keys::fs {
namespace {

/// Maps a QFileDevice error to a Keys error code so callers can branch on the
/// category without parsing message text.
ErrorCode codeFor(QFileDevice::FileError error)
{
    switch (error) {
    case QFileDevice::PermissionsError:
        return ErrorCode::PermissionDenied;
    case QFileDevice::OpenError:
    case QFileDevice::ReadError:
    case QFileDevice::WriteError:
    case QFileDevice::FatalError:
    case QFileDevice::ResourceError:
        return ErrorCode::IoError;
    default:
        return ErrorCode::IoError;
    }
}

} // namespace

bool DirEntry::operator<(const DirEntry& other) const
{
    // Directories first: the explorer reads as a tree rather than an alphabetised
    // mix, which is what makes a project's shape visible at a glance.
    if (isDirectory != other.isDirectory) {
        return isDirectory;
    }
    const int order = name.compare(other.name, Qt::CaseInsensitive);
    if (order != 0) {
        return order < 0;
    }
    // Case-insensitive ties broken case-sensitively so the order is total and
    // stable; otherwise "README" and "readme" could swap between listings.
    return name < other.name;
}

QString FileSystem::normalize(const QString& path)
{
    if (path.isEmpty()) {
        return path;
    }

    // Relative input is made absolute against the working directory first.
    // Without this, "." normalises to "." - and a project opened as `keys .`
    // would take "." as its name and compare unequal to the same folder given
    // by its full path.
    //
    // QDir::isAbsolutePath rather than QFileInfo, because this must not touch
    // the disk: normalize() is called on paths that may not exist.
    const QString absolute =
        QDir::isAbsolutePath(path) ? path : QDir::current().absoluteFilePath(path);

    // cleanPath resolves "." and ".." and converts to forward slashes, which is
    // the form Keys uses everywhere so paths compare and hash consistently.
    return QDir::cleanPath(absolute);
}

bool FileSystem::exists(const QString& path)
{
    return QFileInfo::exists(path);
}

bool FileSystem::isDirectory(const QString& path)
{
    return QFileInfo(path).isDir();
}

Result<QString> FileSystem::readTextFile(const QString& path, qint64 maxBytes)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return Err(ErrorCode::NotFound, QStringLiteral("File does not exist"), path);
    }
    if (info.isDir()) {
        return Err(ErrorCode::InvalidArgument,
                   QStringLiteral("Path is a directory, not a file"), path);
    }
    if (info.size() > maxBytes) {
        // Fail with a number the user can act on rather than exhausting memory.
        return Err(ErrorCode::NotSupported,
                   QStringLiteral("File is %1 MB, which is above the %2 MB limit")
                       .arg(info.size() / (1024 * 1024))
                       .arg(maxBytes / (1024 * 1024)),
                   path);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return Err(codeFor(file.error()), file.errorString(), path);
    }
    return QString::fromUtf8(file.readAll());
}

Status FileSystem::writeTextFile(const QString& path, const QString& content)
{
    const QFileInfo info(path);
    if (!info.dir().exists() && !QDir().mkpath(info.absolutePath())) {
        return Err(ErrorCode::IoError,
                   QStringLiteral("Could not create the containing directory"), path);
    }

    // QSaveFile writes to a temporary and renames on commit. A crash or power loss
    // mid-write leaves the previous contents intact rather than a truncated file -
    // the difference between an inconvenience and lost work.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return Err(codeFor(file.error()), file.errorString(), path);
    }
    if (file.write(content.toUtf8()) == -1) {
        return Err(codeFor(file.error()), file.errorString(), path);
    }
    if (!file.commit()) {
        return Err(codeFor(file.error()), file.errorString(), path);
    }
    return Ok();
}

Result<QList<DirEntry>> FileSystem::listDirectory(const QString& path,
                                                  const core::CancellationToken& token)
{
    QDir dir(path);
    if (!dir.exists()) {
        return Err(ErrorCode::NotFound, QStringLiteral("Directory does not exist"), path);
    }

    QList<DirEntry> entries;

    // Hidden entries are included: a developer's project is full of dotfiles that
    // matter (.gitignore, .env), and hiding them by default makes the explorer
    // lie about what is on disk. Filtering is the explorer's decision, not this
    // layer's.
    const QFileInfoList infos = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::NoSort);

    entries.reserve(infos.size());
    for (const QFileInfo& info : infos) {
        if (token.isCancelled()) {
            return Err(ErrorCode::Cancelled,
                       QStringLiteral("Directory listing was cancelled"), path);
        }

        DirEntry entry;
        entry.name = info.fileName();
        entry.path = normalize(info.absoluteFilePath());
        entry.isDirectory = info.isDir();
        entry.isSymLink = info.isSymLink();
        entry.size = info.size();
        entry.modified = info.lastModified();
        entries.append(std::move(entry));
    }

    std::sort(entries.begin(), entries.end());
    return entries;
}

Status FileSystem::createDirectory(const QString& path)
{
    if (exists(path)) {
        return Err(ErrorCode::AlreadyExists,
                   QStringLiteral("A file or folder with this name already exists"), path);
    }
    if (!QDir().mkpath(path)) {
        return Err(ErrorCode::IoError, QStringLiteral("Could not create the folder"), path);
    }
    return Ok();
}

Status FileSystem::createFile(const QString& path)
{
    if (exists(path)) {
        return Err(ErrorCode::AlreadyExists,
                   QStringLiteral("A file or folder with this name already exists"), path);
    }

    const QFileInfo info(path);
    if (!info.dir().exists() && !QDir().mkpath(info.absolutePath())) {
        return Err(ErrorCode::IoError,
                   QStringLiteral("Could not create the containing directory"), path);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return Err(codeFor(file.error()), file.errorString(), path);
    }
    file.close();
    return Ok();
}

Status FileSystem::moveToTrash(const QString& path)
{
    if (!exists(path)) {
        return Err(ErrorCode::NotFound, QStringLiteral("Path does not exist"), path);
    }

    QFile file(path);
    if (file.moveToTrash()) {
        return Ok();
    }

    // Deliberately no permanent-delete fallback. If the platform cannot trash the
    // file, silently destroying it instead would turn a recoverable action into an
    // unrecoverable one without the user ever being told.
    return Err(codeFor(file.error()),
               file.errorString().isEmpty()
                   ? QStringLiteral("Could not move to trash")
                   : file.errorString(),
               path);
}

Status FileSystem::rename(const QString& from, const QString& to)
{
    if (!exists(from)) {
        return Err(ErrorCode::NotFound, QStringLiteral("Path does not exist"), from);
    }
    if (exists(to)) {
        // Renaming over an existing file would destroy it. Refuse and let the
        // caller decide, rather than choosing data loss on the user's behalf.
        return Err(ErrorCode::AlreadyExists,
                   QStringLiteral("A file or folder with this name already exists"), to);
    }

    const QFileInfo info(to);
    if (!info.dir().exists() && !QDir().mkpath(info.absolutePath())) {
        return Err(ErrorCode::IoError,
                   QStringLiteral("Could not create the destination directory"), to);
    }

    if (!QFile::rename(from, to)) {
        return Err(ErrorCode::IoError, QStringLiteral("Could not rename"), from);
    }
    return Ok();
}

} // namespace keys::fs
