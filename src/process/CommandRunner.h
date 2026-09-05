#pragma once

#include "core/Result.h"

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace keys::process {

/// What a finished command produced.
struct CommandResult {
    int exitCode = -1;
    QString standardOutput;
    QString standardError;

    /// True when the process ran to completion with exit code 0. A command can
    /// fail two ways — it never started, or it ran and returned non-zero — and
    /// callers usually care about the difference, so the distinction survives
    /// rather than collapsing into one boolean.
    [[nodiscard]] bool succeeded() const { return exitCode == 0; }

    /// stderr if the command wrote any, otherwise stdout. Git reports failures on
    /// stderr but not always, so this is what a caller shows the user.
    [[nodiscard]] QString errorText() const;
};

/// How a command is run.
struct CommandOptions {
    QString workingDirectory;

    /// Extra environment entries applied over the inherited environment, as
    /// `KEY=value`.
    QStringList environment;

    /// How long to wait before killing the child. A hung command must not hold a
    /// worker thread forever: a git operation waiting on a credential prompt that
    /// will never be answered would otherwise leak a thread per attempt.
    int timeoutMs = 30000;
};

/// Runs a child process to completion and captures its output.
///
/// **Blocking by design.** This runs the process synchronously on the calling
/// thread, which must therefore never be the UI thread. Callers post it to the
/// TaskScheduler; making it asynchronous here would duplicate the scheduler's
/// job and give every caller two ways to get threading wrong instead of one.
///
/// **Separate from Pty.** A PTY exists so an interactive shell believes it has a
/// terminal. A tool being scripted wants the opposite: no terminal, so it emits
/// plain machine-readable output instead of colour codes and progress bars, and
/// so it fails immediately rather than prompting for input nobody will type.
class CommandRunner {
public:
    /// Runs `program` with `arguments`. Returns a failure only when the process
    /// could not be run at all — a non-zero exit is a successful run of a
    /// command that failed, which is reported through CommandResult.
    [[nodiscard]] static core::Result<CommandResult> run(const QString& program,
                                                         const QStringList& arguments,
                                                         const CommandOptions& options = {});

private:
    /// The environment a scripted tool should see: the user's own, plus the
    /// entries that stop a tool trying to be interactive.
    [[nodiscard]] static QProcessEnvironment scriptedEnvironment(
        const QStringList& extra);
};

} // namespace keys::process
