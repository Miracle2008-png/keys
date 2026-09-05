#include "vcs/GitTypes.h"

#include <algorithm>

namespace keys::vcs {

int RepositoryStatus::stagedCount() const
{
    return static_cast<int>(std::count_if(
        changes.cbegin(), changes.cend(),
        [](const FileChange& change) { return change.hasStagedChange(); }));
}

int RepositoryStatus::unstagedCount() const
{
    return static_cast<int>(std::count_if(
        changes.cbegin(), changes.cend(), [](const FileChange& change) {
            return change.hasUnstagedChange()
                   || change.unstaged == FileStatus::Untracked;
        }));
}

bool RepositoryStatus::hasConflicts() const
{
    return std::any_of(changes.cbegin(), changes.cend(),
                       [](const FileChange& change) { return change.isConflicted(); });
}

FileStatus fileStatusFromCode(QChar code)
{
    switch (code.toLatin1()) {
    case 'M':
        return FileStatus::Modified;
    case 'A':
        return FileStatus::Added;
    case 'D':
        return FileStatus::Deleted;
    case 'R':
        return FileStatus::Renamed;
    case 'C':
        return FileStatus::Copied;
    case '?':
        return FileStatus::Untracked;
    case '!':
        return FileStatus::Ignored;
    case 'U':
        return FileStatus::Conflicted;
    // 'T' is a type change (file to symlink). It is a modification as far as
    // anything Keys does with it is concerned.
    case 'T':
        return FileStatus::Modified;
    case ' ':
    default:
        return FileStatus::Unmodified;
    }
}

QString fileStatusLabel(FileStatus status)
{
    switch (status) {
    case FileStatus::Modified:
        return QStringLiteral("Modified");
    case FileStatus::Added:
        return QStringLiteral("Added");
    case FileStatus::Deleted:
        return QStringLiteral("Deleted");
    case FileStatus::Renamed:
        return QStringLiteral("Renamed");
    case FileStatus::Copied:
        return QStringLiteral("Copied");
    case FileStatus::Untracked:
        return QStringLiteral("Untracked");
    case FileStatus::Ignored:
        return QStringLiteral("Ignored");
    case FileStatus::Conflicted:
        return QStringLiteral("Conflicted");
    case FileStatus::Unmodified:
        break;
    }
    return QString();
}

QString fileStatusLetter(FileStatus status)
{
    switch (status) {
    case FileStatus::Modified:
        return QStringLiteral("M");
    case FileStatus::Added:
        return QStringLiteral("A");
    case FileStatus::Deleted:
        return QStringLiteral("D");
    case FileStatus::Renamed:
        return QStringLiteral("R");
    case FileStatus::Copied:
        return QStringLiteral("C");
    case FileStatus::Untracked:
        return QStringLiteral("U");
    case FileStatus::Ignored:
        return QStringLiteral("I");
    case FileStatus::Conflicted:
        return QStringLiteral("!");
    case FileStatus::Unmodified:
        break;
    }
    return QString();
}

} // namespace keys::vcs
