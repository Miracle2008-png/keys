#pragma once

#include "buildrun/Task.h"
#include "core/Result.h"

#include <QObject>
#include <QString>

#include <memory>

class QProcess;

namespace keys::buildrun {

/// Runs one task at a time, streaming its output as it arrives.
///
/// **Streaming, not captured.** A build's output is the point: a developer reads
/// it while it runs, and a compile that takes ninety seconds must not show a
/// blank pane for ninety seconds and then everything at once. So this is built
/// on QProcess's readyRead signals rather than on CommandRunner, which waits for
/// completion by design.
///
/// **No PTY.** A build tool is being scripted, not driven interactively: it
/// should emit plain text rather than progress bars that assume a cursor, and it
/// must not stop to ask a question nobody will answer. Pipes give exactly that,
/// and they work today — the terminal's ConPTY attachment does not.
///
/// **One at a time.** Two builds writing to the same output directory corrupt
/// each other, and two consoles interleaving into one pane is unreadable.
/// Starting a task while one runs is refused; the caller stops the running one
/// first, which is an explicit decision rather than a silent queue.
class TaskRunner : public QObject {
    Q_OBJECT

public:
    explicit TaskRunner(QObject* parent = nullptr);
    ~TaskRunner() override;

    /// Starts `task` in `projectRoot`. Fails if a task is already running, if
    /// the task is malformed, or if the program cannot be started — the last of
    /// which is the common case (a toolchain that is not installed) and is
    /// reported with the program's name so the message is actionable.
    core::Status start(const Task& task, const QString& projectRoot);

    /// Asks the running task to stop, escalating to a kill if it ignores that.
    /// Safe to call when nothing is running.
    void stop();

    [[nodiscard]] bool isRunning() const;

    /// The task currently running, or the last one that ran.
    [[nodiscard]] const Task& currentTask() const { return m_task; }

    /// Milliseconds the last run took, or the elapsed time so far. Shown in the
    /// console's summary, because "how long does the build take" is a question
    /// developers ask constantly.
    [[nodiscard]] qint64 elapsedMs() const;

signals:
    void started(const QString& taskName, const QString& commandLine);

    /// One or more complete lines of output. Lines rather than raw chunks: a
    /// read can split a line anywhere, and every consumer would otherwise have
    /// to reassemble them.
    void outputReceived(const QStringList& lines, bool isError);

    /// The task ended. `wasStopped` distinguishes the user stopping it from the
    /// process ending on its own; `exitCode` is meaningful only when it did not
    /// crash, which `crashed` reports separately - a crashed build and a build
    /// the user stopped look identical from an exit code alone, and they mean
    /// entirely different things.
    void finished(int exitCode, bool wasStopped, bool crashed);

    /// The process could not be started at all, as opposed to running and
    /// failing. Different from a non-zero exit and shown differently.
    void failedToStart(const QString& message);

    void runningChanged();

private:
    /// Splits buffered output into complete lines, keeping any partial tail for
    /// the next read.
    void drain(bool isError);

    void onFinished(int exitCode, bool crashed);

    std::unique_ptr<QProcess> m_process;
    Task m_task;

    /// Partial lines held between reads. Separate per stream because stdout and
    /// stderr arrive independently and interleaving their fragments would
    /// produce corrupted lines.
    QString m_pendingOutput;
    QString m_pendingError;

    qint64 m_startedAtMs = 0;
    qint64 m_finishedAtMs = 0;

    /// Set by stop(), so the completion can report that the task was stopped
    /// rather than that it failed - which is what the user needs to know.
    bool m_stopping = false;

    /// How long to wait for a stopped process to exit before killing it.
    static constexpr int kTerminateGraceMs = 2000;
};

} // namespace keys::buildrun
