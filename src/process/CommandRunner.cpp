#include "process/CommandRunner.h"

#include "core/Log.h"

#include <QProcess>

namespace keys::process {

QString CommandResult::errorText() const
{
    const QString trimmedError = standardError.trimmed();
    return trimmedError.isEmpty() ? standardOutput.trimmed() : trimmedError;
}

QProcessEnvironment CommandRunner::scriptedEnvironment(const QStringList& extra)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // A tool that decides it can ask the user a question will block forever
    // here: there is no terminal to answer on and no UI attached to its stdin.
    // These say "you are being scripted" in the ways the tools we run listen to.
    //
    // GIT_TERMINAL_PROMPT=0 makes git fail instead of prompting for credentials.
    // GIT_ASKPASS/SSH_ASKPASS empty stops it launching a graphical prompt as a
    // fallback. GIT_OPTIONAL_LOCKS=0 keeps read-only commands from taking the
    // index lock, so a background status cannot block the user's own commit.
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    environment.insert(QStringLiteral("GIT_ASKPASS"), QString());
    environment.insert(QStringLiteral("SSH_ASKPASS"), QString());
    environment.insert(QStringLiteral("GIT_OPTIONAL_LOCKS"), QStringLiteral("0"));

    // Machine-readable output: a locale that does not translate messages, and no
    // pager, which would otherwise wait for a keypress that never comes.
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("GIT_PAGER"), QStringLiteral("cat"));

    for (const QString& entry : extra) {
        const int separator = entry.indexOf(QLatin1Char('='));
        if (separator > 0) {
            environment.insert(entry.left(separator), entry.mid(separator + 1));
        }
    }
    return environment;
}

core::Result<CommandResult> CommandRunner::run(const QString& program,
                                               const QStringList& arguments,
                                               const CommandOptions& options)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessEnvironment(scriptedEnvironment(options.environment));

    if (!options.workingDirectory.isEmpty()) {
        process.setWorkingDirectory(options.workingDirectory);
    }

    // Nothing is ever written to the child. Closing stdin means a tool that does
    // try to read input sees EOF and gives up immediately, rather than waiting
    // for the timeout.
    process.start(QIODevice::ReadOnly);

    if (!process.waitForStarted(options.timeoutMs)) {
        return core::Err(core::ErrorCode::ProcessFailed,
                         QStringLiteral("Could not start the process: %1")
                             .arg(process.errorString()),
                         program);
    }

    if (!process.waitForFinished(options.timeoutMs)) {
        // Terminate first so the child can clean up; kill only if it ignores
        // that. Leaving it running would leak a process per timeout.
        process.terminate();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished(1000);
        }
        return core::Err(core::ErrorCode::Timeout,
                         QStringLiteral("The process did not finish within %1 ms")
                             .arg(options.timeoutMs),
                         program + QLatin1Char(' ') + arguments.join(QLatin1Char(' ')));
    }

    if (process.exitStatus() != QProcess::NormalExit) {
        return core::Err(core::ErrorCode::ProcessFailed,
                         QStringLiteral("The process crashed"),
                         program);
    }

    CommandResult result;
    result.exitCode = process.exitCode();

    // Git speaks UTF-8 regardless of the console code page, and paths in a
    // repository can hold any of it. Decoding as the local 8-bit encoding would
    // corrupt non-ASCII file names on Windows.
    result.standardOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.standardError = QString::fromUtf8(process.readAllStandardError());

    return result;
}

} // namespace keys::process
