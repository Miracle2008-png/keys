#include "vcs/GitClient.h"

#include "core/Log.h"
#include "process/CommandRunner.h"

#include <QDir>
#include <QFileInfo>

using keys::process::CommandOptions;
using keys::process::CommandResult;
using keys::process::CommandRunner;

namespace keys::vcs {
namespace {

/// The program name. Resolved through PATH rather than an absolute path so the
/// user's own git — Git for Windows, a version manager, a corporate build — is
/// what runs, exactly as it would in their terminal.
constexpr auto kGit = "git";

/// Long enough for a slow first status on a large repository, short enough that
/// a hung command surfaces rather than wedging a worker forever.
constexpr int kReadTimeoutMs = 30000;

/// Writes can legitimately take longer: a commit runs the user's hooks.
constexpr int kWriteTimeoutMs = 120000;

} // namespace

GitClient::GitClient(QString repositoryRoot) : m_root(std::move(repositoryRoot)) {}

bool GitClient::isGitAvailable()
{
    return !gitVersion().isEmpty();
}

QString GitClient::gitVersion()
{
    CommandOptions options;
    options.timeoutMs = 5000;

    const core::Result<CommandResult> result =
        CommandRunner::run(QLatin1String(kGit), {QStringLiteral("--version")}, options);

    if (!result || !result.value().succeeded()) {
        return QString();
    }
    return result.value().standardOutput.trimmed();
}

core::Result<QString> GitClient::findRepositoryRoot(const QString& path)
{
    if (path.isEmpty()) {
        return core::Err(core::ErrorCode::InvalidArgument,
                         QStringLiteral("No path given"));
    }

    CommandOptions options;
    options.workingDirectory = path;
    options.timeoutMs = 10000;

    // --show-toplevel asks git itself rather than walking for a .git directory:
    // it handles worktrees, submodules and .git files, which a manual walk gets
    // subtly wrong.
    const core::Result<CommandResult> result = CommandRunner::run(
        QLatin1String(kGit),
        {QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")}, options);

    if (!result) {
        return result.error();
    }
    if (!result.value().succeeded()) {
        // Not a repository. Expected for most projects, so this is a plain
        // NotFound rather than something the UI reports as a failure.
        return core::Err(core::ErrorCode::NotFound,
                         QStringLiteral("Not a git repository"), path);
    }

    const QString root = result.value().standardOutput.trimmed();
    if (root.isEmpty()) {
        return core::Err(core::ErrorCode::NotFound,
                         QStringLiteral("Not a git repository"), path);
    }
    return QDir::cleanPath(root);
}

core::Result<QString> GitClient::run(const QStringList& arguments) const
{
    CommandOptions options;
    options.workingDirectory = m_root;
    options.timeoutMs = kReadTimeoutMs;

    const core::Result<CommandResult> result =
        CommandRunner::run(QLatin1String(kGit), arguments, options);
    if (!result) {
        return result.error();
    }

    const CommandResult& command = result.value();
    if (!command.succeeded()) {
        // Git's own message, which is far more useful than anything this layer
        // could invent from an exit code.
        return core::Err(core::ErrorCode::ProcessFailed, command.errorText(),
                         QStringLiteral("git ") + arguments.join(QLatin1Char(' ')));
    }
    return command.standardOutput;
}

RepositoryStatus GitClient::parseStatus(const QString& output)
{
    RepositoryStatus status;

    // -z separates records with NUL rather than newline, so a path containing a
    // newline cannot be mistaken for a record boundary. Git quotes such paths in
    // the non-z form, which would then have to be unquoted correctly - this
    // avoids the problem instead of solving it.
    const QStringList records = output.split(QLatin1Char('\0'), Qt::SkipEmptyParts);

    for (int i = 0; i < records.size(); ++i) {
        const QString& record = records.at(i);
        if (record.isEmpty()) {
            continue;
        }

        const QChar kind = record.at(0);

        // Header lines: "# branch.head main", "# branch.ab +1 -2".
        if (kind == QLatin1Char('#')) {
            const QStringList fields = record.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (fields.size() < 3) {
                continue;
            }
            const QString& key = fields.at(1);

            if (key == QLatin1String("branch.head")) {
                // Literally "(detached)" when there is no branch.
                const QString head = fields.at(2);
                status.branch = head == QLatin1String("(detached)") ? QString() : head;
            } else if (key == QLatin1String("branch.oid")) {
                status.headCommit = fields.at(2).left(7);
            } else if (key == QLatin1String("branch.upstream")) {
                status.upstream = fields.at(2);
            } else if (key == QLatin1String("branch.ab") && fields.size() >= 4) {
                // "+3 -1": ahead of upstream by 3, behind by 1.
                status.ahead = fields.at(2).mid(1).toInt();
                status.behind = fields.at(3).mid(1).toInt();
            }
            continue;
        }

        FileChange change;

        if (kind == QLatin1Char('?')) {
            // "? path"
            change.path = record.mid(2);
            change.unstaged = FileStatus::Untracked;
            status.changes.push_back(std::move(change));
            continue;
        }

        if (kind == QLatin1Char('!')) {
            continue;   // ignored; not asked for, and not shown if it arrives
        }

        if (kind == QLatin1Char('u')) {
            // Unmerged: "u XY N... <path>". Both sides are conflicted; the
            // individual stage codes are not something the UI acts on.
            const int pathStart = record.indexOf(QLatin1Char('\t'));
            change.path = pathStart >= 0 ? record.mid(pathStart + 1)
                                         : record.section(QLatin1Char(' '), 10);
            change.staged = FileStatus::Conflicted;
            change.unstaged = FileStatus::Conflicted;
            status.changes.push_back(std::move(change));
            continue;
        }

        if (kind != QLatin1Char('1') && kind != QLatin1Char('2')) {
            continue;
        }

        // "1 XY sub mH mI mW hH hI <path>"
        // "2 XY sub mH mI mW hH hI X<score> <path>\0<origPath>"
        // Fields are space-separated up to the path, which may itself contain
        // spaces - so the path is taken as the remainder after a fixed count.
        const QStringList fields = record.split(QLatin1Char(' '));
        if (fields.size() < 9) {
            continue;
        }

        const QString& codes = fields.at(1);
        if (codes.size() < 2) {
            continue;
        }
        change.staged = fileStatusFromCode(codes.at(0));
        change.unstaged = fileStatusFromCode(codes.at(1));

        const int fixedFields = kind == QLatin1Char('2') ? 9 : 8;
        change.path = fields.mid(fixedFields).join(QLatin1Char(' '));

        // A rename's original path is the next NUL-separated record, not part of
        // this one.
        if (kind == QLatin1Char('2') && i + 1 < records.size()) {
            change.originalPath = records.at(++i);
        }

        status.changes.push_back(std::move(change));
    }

    return status;
}

void GitClient::detectOperation(RepositoryStatus& status) const
{
    // Git records an in-progress operation as files under .git. Asking the
    // filesystem is both cheaper and more reliable than parsing the advice text
    // git prints, which is localised and version-dependent.
    const QDir gitDir(QDir(m_root).filePath(QStringLiteral(".git")));

    struct Marker {
        const char* path;
        const char* name;
    };
    static constexpr Marker markers[] = {
        {"MERGE_HEAD", "merge"},
        {"rebase-merge", "rebase"},
        {"rebase-apply", "rebase"},
        {"CHERRY_PICK_HEAD", "cherry-pick"},
        {"REVERT_HEAD", "revert"},
        {"BISECT_LOG", "bisect"},
    };

    for (const Marker& marker : markers) {
        if (QFileInfo::exists(gitDir.filePath(QLatin1String(marker.path)))) {
            status.operationInProgress = true;
            status.operationName = QLatin1String(marker.name);
            return;
        }
    }
}

core::Result<RepositoryStatus> GitClient::status() const
{
    const core::Result<QString> output = run({
        QStringLiteral("status"),
        QStringLiteral("--porcelain=v2"),
        QStringLiteral("--branch"),
        QStringLiteral("-z"),
        // Untracked files individually rather than a collapsed directory entry,
        // so the UI can stage one file from a new folder.
        QStringLiteral("--untracked-files=all"),
    });

    if (!output) {
        return output.error();
    }

    RepositoryStatus status = parseStatus(output.value());
    detectOperation(status);
    return status;
}

core::Result<std::vector<Commit>> GitClient::log(int limit) const
{
    // Unit separator between fields and record separator between commits: both
    // are control characters that cannot appear in a commit subject or an
    // author name, so no escaping is needed.
    const core::Result<QString> output = run({
        QStringLiteral("log"),
        QStringLiteral("--max-count=%1").arg(std::max(1, limit)),
        QStringLiteral("--format=%H%x1f%h%x1f%s%x1f%an%x1f%ae%x1f%aI%x1e"),
    });

    if (!output) {
        return output.error();
    }

    std::vector<Commit> commits;
    const QStringList records =
        output.value().split(QLatin1Char('\x1e'), Qt::SkipEmptyParts);

    commits.reserve(static_cast<size_t>(records.size()));
    for (const QString& record : records) {
        const QStringList fields = record.trimmed().split(QLatin1Char('\x1f'));
        if (fields.size() < 6) {
            continue;
        }
        Commit commit;
        commit.hash = fields.at(0);
        commit.shortHash = fields.at(1);
        commit.subject = fields.at(2);
        commit.author = fields.at(3);
        commit.authorEmail = fields.at(4);
        commit.date = fields.at(5);
        commits.push_back(std::move(commit));
    }
    return commits;
}

core::Result<QString> GitClient::diff(const QString& path, bool staged) const
{
    QStringList arguments{QStringLiteral("diff")};
    if (staged) {
        arguments << QStringLiteral("--cached");
    }
    // No colour and no external diff tool: this output is parsed and rendered by
    // Keys, not shown raw.
    arguments << QStringLiteral("--no-color") << QStringLiteral("--no-ext-diff");

    if (!path.isEmpty()) {
        // Everything after -- is a path, so a file named "--cached" cannot be
        // read as an option.
        arguments << QStringLiteral("--") << path;
    }
    return run(arguments);
}

core::Result<QStringList> GitClient::branches() const
{
    const core::Result<QString> output = run({
        QStringLiteral("for-each-ref"),
        QStringLiteral("--format=%(refname:short)"),
        QStringLiteral("refs/heads/"),
    });

    if (!output) {
        return output.error();
    }
    return output.value().split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

core::Status GitClient::stage(const QStringList& paths) const
{
    QStringList arguments{QStringLiteral("add")};
    if (paths.isEmpty()) {
        arguments << QStringLiteral("--all");
    } else {
        arguments << QStringLiteral("--") << paths;
    }

    const core::Result<QString> result = run(arguments);
    return result ? core::Ok() : core::Status(result.error());
}

core::Status GitClient::unstage(const QStringList& paths) const
{
    // restore --staged rather than "reset HEAD": it does the same thing and
    // says what it means, and it works on a repository with no commits yet,
    // where HEAD does not resolve.
    QStringList arguments{QStringLiteral("restore"), QStringLiteral("--staged")};
    if (paths.isEmpty()) {
        arguments << QStringLiteral(".");
    } else {
        arguments << QStringLiteral("--") << paths;
    }

    const core::Result<QString> result = run(arguments);
    return result ? core::Ok() : core::Status(result.error());
}

core::Status GitClient::discard(const QStringList& paths) const
{
    if (paths.isEmpty()) {
        // Refused rather than interpreted: "discard everything" is too
        // destructive to be the meaning of an empty argument, which is what an
        // empty selection would produce by accident.
        return core::Err(core::ErrorCode::InvalidArgument,
                         QStringLiteral("No paths given to discard"));
    }

    // --worktree is the default, but stating it makes clear this touches the
    // working tree only and leaves anything staged alone.
    QStringList arguments{QStringLiteral("restore"), QStringLiteral("--worktree"),
                          QStringLiteral("--")};
    arguments << paths;

    const core::Result<QString> result = run(arguments);
    return result ? core::Ok() : core::Status(result.error());
}

core::Status GitClient::commit(const QString& message) const
{
    if (message.trimmed().isEmpty()) {
        return core::Err(core::ErrorCode::InvalidArgument,
                         QStringLiteral("A commit needs a message"));
    }

    CommandOptions options;
    options.workingDirectory = m_root;
    options.timeoutMs = kWriteTimeoutMs;   // hooks can be slow

    // The message is passed as one argument rather than through a file or
    // stdin: -m takes it verbatim, including newlines, with no quoting for a
    // shell to get wrong because no shell is involved.
    const core::Result<CommandResult> result = CommandRunner::run(
        QLatin1String(kGit),
        {QStringLiteral("commit"), QStringLiteral("--message"), message}, options);

    if (!result) {
        return result.error();
    }
    if (!result.value().succeeded()) {
        return core::Err(core::ErrorCode::ProcessFailed, result.value().errorText(),
                         QStringLiteral("git commit"));
    }
    return core::Ok();
}

} // namespace keys::vcs
