#include "buildrun/TaskRunner.h"

#include "core/Log.h"

#include <QDateTime>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>

namespace keys::buildrun {

TaskRunner::TaskRunner(QObject* parent) : QObject(parent) {}

TaskRunner::~TaskRunner()
{
    // A build left running after Keys exits keeps writing to a directory nobody
    // is watching, and on Windows holds file locks that block the next build.
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(kTerminateGraceMs);
    }
}

bool TaskRunner::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

qint64 TaskRunner::elapsedMs() const
{
    if (m_startedAtMs == 0) {
        return 0;
    }
    const qint64 end = isRunning() ? QDateTime::currentMSecsSinceEpoch() : m_finishedAtMs;
    return end - m_startedAtMs;
}

core::Status TaskRunner::start(const Task& task, const QString& projectRoot)
{
    if (isRunning()) {
        return core::Err(core::ErrorCode::AlreadyExists,
                         QStringLiteral("A task is already running"), m_task.name);
    }
    if (!task.isValid()) {
        return core::Err(core::ErrorCode::InvalidArgument,
                         QStringLiteral("The task has no name or program"), task.name);
    }

    m_task = task;
    m_stopping = false;
    m_pendingOutput.clear();
    m_pendingError.clear();
    m_finishedAtMs = 0;

    m_process = std::make_unique<QProcess>();

    const QString workingDirectory =
        task.workingDirectory.isEmpty()
            ? projectRoot
            : QDir(projectRoot).filePath(task.workingDirectory);
    m_process->setWorkingDirectory(workingDirectory);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    // A build tool that detects a terminal draws progress bars with cursor
    // movement, which is noise in a pane that only appends lines. These are the
    // variables the common toolchains read.
    environment.insert(QStringLiteral("TERM"), QStringLiteral("dumb"));
    environment.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    environment.insert(QStringLiteral("CI"), QStringLiteral("1"));

    for (const QString& entry : task.environment) {
        const int separator = entry.indexOf(QLatin1Char('='));
        if (separator > 0) {
            environment.insert(entry.left(separator), entry.mid(separator + 1));
        }
    }
    m_process->setProcessEnvironment(environment);

    connect(m_process.get(), &QProcess::readyReadStandardOutput, this,
            [this] { drain(false); });
    connect(m_process.get(), &QProcess::readyReadStandardError, this,
            [this] { drain(true); });

    connect(m_process.get(), &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
                onFinished(exitCode, status == QProcess::CrashExit);
            });

    connect(m_process.get(), &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart) {
                    return;   // other errors surface through finished()
                }
                // By far the most common failure: the toolchain is not
                // installed. Naming the program is what makes the message
                // actionable rather than "the process failed".
                const QString message =
                    QStringLiteral("Could not start '%1'. Is it installed and on your PATH?")
                        .arg(m_task.program);
                qCWarning(lcCore) << message;

                m_finishedAtMs = QDateTime::currentMSecsSinceEpoch();
                emit failedToStart(message);
                emit runningChanged();
            });

    m_startedAtMs = QDateTime::currentMSecsSinceEpoch();

    // Read-only: nothing is ever written to a build tool, and closing stdin
    // means one that tries to read a prompt sees EOF and gives up rather than
    // hanging forever.
    m_process->start(task.program, task.arguments, QIODevice::ReadOnly);

    emit started(task.name, task.commandLine());
    emit runningChanged();
    return core::Ok();
}

void TaskRunner::stop()
{
    if (!isRunning()) {
        return;
    }

    m_stopping = true;

    // Terminate first so the tool can clean up its temporary files; kill only if
    // it ignores that. A build killed mid-write can leave a corrupt object file
    // that the next build happily links.
    m_process->terminate();
    if (!m_process->waitForFinished(kTerminateGraceMs)) {
        qCWarning(lcCore) << m_task.program << "ignored terminate; killing";
        m_process->kill();
        m_process->waitForFinished(kTerminateGraceMs);
    }
}

void TaskRunner::drain(bool isError)
{
    if (!m_process) {
        return;
    }

    // UTF-8 regardless of the console code page: compilers emit UTF-8, and paths
    // in a project can hold any of it.
    const QString chunk = QString::fromUtf8(isError ? m_process->readAllStandardError()
                                                    : m_process->readAllStandardOutput());
    if (chunk.isEmpty()) {
        return;
    }

    QString& pending = isError ? m_pendingError : m_pendingOutput;
    pending += chunk;

    // A read can split a line anywhere, so only complete lines are emitted and
    // the tail is held for the next read. Every consumer would otherwise have to
    // reassemble them, and each would get it slightly wrong.
    QStringList lines;
    int start = 0;
    while (true) {
        const int newline = pending.indexOf(QLatin1Char('\n'), start);
        if (newline < 0) {
            break;
        }
        QString line = pending.mid(start, newline - start);
        // Tools that emit CRLF are common on Windows; a trailing carriage return
        // would otherwise render as a stray glyph.
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        lines.append(line);
        start = newline + 1;
    }

    if (start > 0) {
        pending = pending.mid(start);
    }

    if (!lines.isEmpty()) {
        emit outputReceived(lines, isError);
    }
}

void TaskRunner::onFinished(int exitCode, bool crashed)
{
    // Whatever was buffered without a trailing newline is still output the user
    // needs - a compiler's last line often has no newline.
    for (const bool isError : {false, true}) {
        QString& pending = isError ? m_pendingError : m_pendingOutput;
        if (!pending.isEmpty()) {
            emit outputReceived({pending}, isError);
            pending.clear();
        }
    }

    m_finishedAtMs = QDateTime::currentMSecsSinceEpoch();

    const bool wasStopped = m_stopping;
    m_stopping = false;

    // Terminating a process is reported by Windows as a crash, so a stop wins:
    // the user knows they stopped it, and calling that a crash would be wrong.
    emit finished(exitCode, wasStopped, crashed && !wasStopped);
    emit runningChanged();
}

} // namespace keys::buildrun
