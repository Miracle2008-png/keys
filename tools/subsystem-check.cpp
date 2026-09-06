// Runs the subsystems that shell out, against real tools, and reports what
// they did.
//
// Build/run, git and the language servers all depend on an external program
// being present and behaving. Their unit tests use fixtures; this uses whatever
// is actually installed, which is the only way to find out that a parser is
// keyed to an output format the installed version no longer produces, or that a
// program Keys expects on PATH is not there.
//
// A missing toolchain is reported as skipped, not failed: the machine not
// having Go installed is not a defect in Keys.
//
//     cmake --build build --target keys_subsystem_check
//     build/bin/keys_subsystem_check

#include "buildrun/Task.h"
#include "buildrun/TaskRunner.h"
#include "vcs/GitClient.h"
#include "vcs/GitTypes.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

using namespace keys;

namespace {

int g_failures = 0;
int g_skipped = 0;

void report(QTextStream& out, const QString& name, bool ok, const QString& detail)
{
    out << (ok ? QStringLiteral("  ok  ") : QStringLiteral("  FAIL")) << " "
        << name.leftJustified(34) << "  " << detail << "\n";
    if (!ok) {
        ++g_failures;
    }
}

void skip(QTextStream& out, const QString& name, const QString& why)
{
    out << "  skip " << name.leftJustified(34) << "  " << why << "\n";
    ++g_skipped;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QString root = QDir::current().absolutePath();
    out << "project: " << root << "\n\n";

    // ---- The programs Keys shells out to ---------------------------------
    out << "-- toolchain --\n";
    for (const auto& [name, program] : {
             std::pair{QStringLiteral("git"), QStringLiteral("git")},
             std::pair{QStringLiteral("cmake"), QStringLiteral("cmake")},
             std::pair{QStringLiteral("clangd (C/C++ LSP)"), QStringLiteral("clangd")},
             std::pair{QStringLiteral("pyright (Python LSP)"), QStringLiteral("pyright-langserver")},
             std::pair{QStringLiteral("rust-analyzer (Rust LSP)"), QStringLiteral("rust-analyzer")},
             std::pair{QStringLiteral("gopls (Go LSP)"), QStringLiteral("gopls")},
         }) {
        const QString found = QStandardPaths::findExecutable(program);
        if (found.isEmpty()) {
            skip(out, name, QStringLiteral("not on PATH"));
        } else {
            report(out, name, true, found);
        }
    }

    // ---- git ---------------------------------------------------------------
    out << "\n-- source control --\n";
    if (!vcs::GitClient::isGitAvailable()) {
        skip(out, QStringLiteral("git status"), QStringLiteral("git is not installed"));
    } else {
        report(out, QStringLiteral("git version"), true, vcs::GitClient::gitVersion());

        const core::Result<QString> found = vcs::GitClient::findRepositoryRoot(root);
        if (!found) {
            skip(out, QStringLiteral("git status"),
                 QStringLiteral("not a repository"));
        } else {
            const vcs::GitClient git(found.value());
            const core::Result<vcs::RepositoryStatus> status = git.status();

            report(out, QStringLiteral("git status"), static_cast<bool>(status),
                   status ? QStringLiteral("branch=%1 changed=%2 ahead=%3")
                                .arg(status.value().branch)
                                .arg(status.value().changes.size())
                                .arg(status.value().ahead)
                          : status.error().toString());

            const core::Result<std::vector<vcs::Commit>> commits = git.log(5);
            report(out, QStringLiteral("git log"), static_cast<bool>(commits),
                   commits ? QStringLiteral("%1 commit(s) read")
                                 .arg(commits.value().size())
                           : commits.error().toString());
        }
    }

    // ---- build/run ---------------------------------------------------------
    out << "\n-- build and run --\n";
    {
        buildrun::TaskRunner runner;

        buildrun::Task task;
        task.name = QStringLiteral("probe");

        // Something every Windows machine has, whose output is unmistakable.
        task.program = QStringLiteral("cmd.exe");
        task.arguments = {QStringLiteral("/c"), QStringLiteral("echo KEYS_TASK_OK")};

        QStringList collected;
        QObject::connect(&runner, &buildrun::TaskRunner::outputReceived,
                         [&collected](const QStringList& lines, bool) {
                             collected += lines;
                         });

        QEventLoop loop;
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);

        int exitCode = -1;
        bool finished = false;
        QObject::connect(&runner, &buildrun::TaskRunner::finished,
                         [&](int code, bool, bool) {
                             exitCode = code;
                             finished = true;
                             loop.quit();
                         });

        if (const core::Status status = runner.start(task, root); !status) {
            report(out, QStringLiteral("run a task"), false, status.error().toString());
        } else {
            loop.exec();
            const bool echoed =
                collected.join(QLatin1Char('\n')).contains(QStringLiteral("KEYS_TASK_OK"));
            report(out, QStringLiteral("run a task"), finished && exitCode == 0 && echoed,
                   QStringLiteral("exit=%1 lines=%2 echoed=%3")
                       .arg(exitCode)
                       .arg(collected.size())
                       .arg(echoed ? QStringLiteral("yes") : QStringLiteral("no")));
        }
    }

    // ---- a task that cannot start -----------------------------------------
    {
        buildrun::TaskRunner runner;

        buildrun::Task task;
        task.name = QStringLiteral("missing");
        task.program = QStringLiteral("keys-no-such-program-exists");

        // The common real case: a toolchain that is not installed. Starting a
        // process is asynchronous, so this arrives as a signal rather than as
        // the return value - and the message has to name the program, or it is
        // not actionable.
        QString message;
        QEventLoop loop;
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);

        QObject::connect(&runner, &buildrun::TaskRunner::failedToStart,
                         [&](const QString& text) { message = text; loop.quit(); });

        if (const core::Status status = runner.start(task, root); !status) {
            report(out, QStringLiteral("a missing program is reported"), false,
                   QStringLiteral("start() refused it, so the signal never fired"));
        } else {
            loop.exec();
            report(out, QStringLiteral("a missing program is reported"),
                   message.contains(task.program),
                   message.isEmpty()
                       ? QStringLiteral("no failedToStart signal arrived")
                       : message);
        }
    }

    out << "\n";
    out << (g_failures == 0
                ? QStringLiteral("VERDICT: every available subsystem worked (%1 skipped).\n")
                      .arg(g_skipped)
                : QStringLiteral("VERDICT: FAILED - %1 problem(s).\n").arg(g_failures));

    return g_failures == 0 ? 0 : 1;
}
