#pragma once

#include <QString>

#include <vector>

namespace keys::vcs {

/// What happened to one file, on one side of the index.
///
/// Git reports a two-letter code per path: the first letter is the staged
/// change, the second the unstaged one. Both are modelled, because a file can be
/// staged as added and then modified again, and an interface that collapsed the
/// two would have to lie about one of them.
enum class FileStatus {
    Unmodified,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Ignored,
    Conflicted,
};

/// One changed path in the working tree.
struct FileChange {
    /// Relative to the repository root, with forward slashes — matching what
    /// git reports and what the explorer displays.
    QString path;

    /// Where a rename came from. Empty unless `staged` is Renamed or Copied.
    QString originalPath;

    FileStatus staged = FileStatus::Unmodified;
    FileStatus unstaged = FileStatus::Unmodified;

    [[nodiscard]] bool hasStagedChange() const
    {
        return staged != FileStatus::Unmodified && staged != FileStatus::Untracked;
    }

    [[nodiscard]] bool hasUnstagedChange() const
    {
        return unstaged != FileStatus::Unmodified;
    }

    [[nodiscard]] bool isConflicted() const
    {
        return staged == FileStatus::Conflicted || unstaged == FileStatus::Conflicted;
    }
};

/// The repository's state at one moment.
struct RepositoryStatus {
    /// The current branch, or empty when the head is detached.
    QString branch;

    /// The commit the head points at, short form. Shown when detached, where
    /// there is no branch name to show instead.
    QString headCommit;

    /// The configured upstream, e.g. "origin/main". Empty when the branch has
    /// none, which is why ahead/behind are meaningless rather than zero.
    QString upstream;

    int ahead = 0;
    int behind = 0;

    /// True while a merge, rebase, cherry-pick or bisect is in progress. The UI
    /// must not offer an ordinary commit in the middle of one.
    bool operationInProgress = false;

    /// Names the operation for the status bar ("merge", "rebase"). Empty when
    /// none is running.
    QString operationName;

    std::vector<FileChange> changes;

    [[nodiscard]] bool hasUpstream() const { return !upstream.isEmpty(); }
    [[nodiscard]] bool isDetached() const { return branch.isEmpty(); }

    [[nodiscard]] bool isClean() const { return changes.empty(); }

    /// How many files have something staged, which is what decides whether a
    /// commit is possible.
    [[nodiscard]] int stagedCount() const;
    [[nodiscard]] int unstagedCount() const;
    [[nodiscard]] bool hasConflicts() const;
};

/// One entry in the history.
struct Commit {
    QString hash;        ///< full hash, for commands
    QString shortHash;   ///< abbreviated, for display
    QString subject;     ///< first line of the message
    QString author;
    QString authorEmail;

    /// ISO-8601, as git emits it. Kept as text rather than parsed into a
    /// QDateTime here so the vcs module stays free of formatting decisions that
    /// belong to the view.
    QString date;
};

/// A single-letter status code from git's porcelain output.
[[nodiscard]] FileStatus fileStatusFromCode(QChar code);

/// A short human label ("Modified", "Added"), for the UI.
[[nodiscard]] QString fileStatusLabel(FileStatus status);

/// The single letter the UI shows beside a path ("M", "A", "U").
[[nodiscard]] QString fileStatusLetter(FileStatus status);

} // namespace keys::vcs
