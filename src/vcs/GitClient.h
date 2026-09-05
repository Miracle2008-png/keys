#pragma once

#include "core/Result.h"
#include "vcs/GitTypes.h"

#include <QString>
#include <QStringList>

namespace keys::vcs {

/// Runs git and parses what it says.
///
/// **Blocking, and deliberately so.** Every call here runs a child process to
/// completion on the calling thread. Repository owns the threading; this layer
/// owns the commands and the parsing, and keeping the two apart is what makes
/// the parsing testable without a scheduler or an event loop.
///
/// **Porcelain formats only.** Git has two kinds of output: what it prints for
/// people, which changes between versions and follows the user's config, and
/// what it prints for scripts, which is documented as stable. Everything here
/// uses the latter — `status --porcelain=v2`, `log --format` with explicit
/// fields, `-z` for NUL-separated paths — because parsing human output would
/// break on a user's `status.showUntrackedFiles` setting or a git upgrade.
class GitClient {
public:
    explicit GitClient(QString repositoryRoot);

    /// Whether a usable `git` exists on PATH. Checked once at startup so the UI
    /// can say so plainly rather than every operation failing separately.
    [[nodiscard]] static bool isGitAvailable();

    /// The version string, for diagnostics. Empty when git is missing.
    [[nodiscard]] static QString gitVersion();

    /// Walks up from `path` looking for a repository. Returns the root, or a
    /// failure when there is none — which is the normal case for a project that
    /// is not under version control, not an error to report loudly.
    [[nodiscard]] static core::Result<QString> findRepositoryRoot(const QString& path);

    [[nodiscard]] const QString& root() const { return m_root; }

    // ---- Reading -----------------------------------------------------------

    [[nodiscard]] core::Result<RepositoryStatus> status() const;

    /// The most recent commits on the current branch, newest first.
    [[nodiscard]] core::Result<std::vector<Commit>> log(int limit = 50) const;

    /// The unified diff for one path. `staged` selects the index against HEAD
    /// rather than the working tree against the index.
    [[nodiscard]] core::Result<QString> diff(const QString& path, bool staged) const;

    /// Local branch names, and the current one.
    [[nodiscard]] core::Result<QStringList> branches() const;

    // ---- Writing -----------------------------------------------------------

    /// Stages paths. An empty list stages everything, matching `git add -A`.
    [[nodiscard]] core::Status stage(const QStringList& paths) const;

    /// Unstages paths, leaving the working tree alone.
    [[nodiscard]] core::Status unstage(const QStringList& paths) const;

    /// Throws away working-tree changes to `paths`. Destructive and
    /// unrecoverable — the UI confirms before calling this.
    [[nodiscard]] core::Status discard(const QStringList& paths) const;

    /// Commits what is staged. Fails when nothing is staged rather than creating
    /// an empty commit.
    [[nodiscard]] core::Status commit(const QString& message) const;

private:
    /// Runs git in the repository and returns stdout, or a failure carrying
    /// git's own error text — which is almost always the most useful thing to
    /// show the user.
    [[nodiscard]] core::Result<QString> run(const QStringList& arguments) const;

    /// Parses `status --porcelain=v2 --branch -z` into a status.
    [[nodiscard]] static RepositoryStatus parseStatus(const QString& output);

    /// Detects an in-progress merge, rebase or similar from the files git
    /// leaves in .git while one is running.
    void detectOperation(RepositoryStatus& status) const;

    QString m_root;
};

} // namespace keys::vcs
