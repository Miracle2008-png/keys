#include "ui/RunPanelModel.h"

#include "core/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>

#include <algorithm>

using keys::buildrun::Problem;
using keys::buildrun::ProblemParser;
using keys::buildrun::ProblemSeverity;
using keys::buildrun::Task;

namespace keys::ui {
namespace {

/// The instance main() publishes for QML; see Theme.cpp for the reasoning.
RunPanelModel* g_instance = nullptr;

/// Formats a duration the way a developer reads a build time: milliseconds
/// below a second, then seconds with one decimal, then minutes.
QString formatDuration(qint64 milliseconds)
{
    if (milliseconds < 1000) {
        return QStringLiteral("%1 ms").arg(milliseconds);
    }
    if (milliseconds < 60000) {
        return QStringLiteral("%1 s").arg(milliseconds / 1000.0, 0, 'f', 1);
    }
    const qint64 minutes = milliseconds / 60000;
    const qint64 seconds = (milliseconds % 60000) / 1000;
    return QStringLiteral("%1 m %2 s").arg(minutes).arg(seconds);
}

} // namespace

void RunPanelModel::setInstance(RunPanelModel* instance)
{
    g_instance = instance;
}

RunPanelModel* RunPanelModel::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Q_ASSERT_X(g_instance, "RunPanelModel::create",
               "RunPanelModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

RunPanelModel::RunPanelModel(buildrun::TaskRunner& runner, project::Project& project,
                             QObject* parent)
    : QAbstractListModel(parent), m_runner(runner), m_project(project)
{
    connect(&m_project, &project::Project::opened, this,
            [this] { reloadTasks(); });
    connect(&m_project, &project::Project::closed, this,
            [this] { reloadTasks(); });

    connect(&m_runner, &buildrun::TaskRunner::started, this,
            [this](const QString& name, const QString& commandLine) {
                // Each run starts from a clean console: reading a failure means
                // scrolling, and finding where the last run ended in a shared
                // log is exactly the friction this avoids.
                clear();

                Line line;
                line.text = commandLine;
                line.isCommand = true;
                appendLine(std::move(line));

                m_summary = tr("Running %1…").arg(name);
                m_lastRunFailed = false;
                emit statusChanged();
            });

    connect(&m_runner, &buildrun::TaskRunner::outputReceived, this,
            &RunPanelModel::appendLines);

    connect(&m_runner, &buildrun::TaskRunner::failedToStart, this,
            [this](const QString& message) {
                Line line;
                line.text = message;
                line.isError = true;
                line.severity = static_cast<int>(ProblemSeverity::Error);
                appendLine(std::move(line));

                m_summary = message;
                m_lastRunFailed = true;
                emit statusChanged();
            });

    connect(&m_runner, &buildrun::TaskRunner::finished, this,
            [this](int exitCode, bool wasStopped, bool crashed) {
                const QString name = m_runner.currentTask().name;
                const QString elapsed = formatDuration(m_runner.elapsedMs());

                if (wasStopped) {
                    m_summary = tr("%1 stopped after %2").arg(name, elapsed);
                    m_lastRunFailed = true;
                } else if (crashed) {
                    m_summary = tr("%1 crashed after %2").arg(name, elapsed);
                    m_lastRunFailed = true;
                } else if (exitCode == 0) {
                    m_summary = tr("%1 succeeded in %2").arg(name, elapsed);
                    m_lastRunFailed = false;
                } else {
                    m_summary = tr("%1 failed with exit code %2 after %3")
                                    .arg(name).arg(exitCode).arg(elapsed);
                    m_lastRunFailed = true;
                }
                emit statusChanged();
            });

    connect(&m_runner, &buildrun::TaskRunner::runningChanged,
            this, &RunPanelModel::runningChanged);

    reloadTasks();
}

QString RunPanelModel::currentTaskName() const
{
    return m_runner.currentTask().name;
}

void RunPanelModel::reloadTasks()
{
    m_tasks = m_project.isOpen() ? buildrun::defaultTasksFor(m_project.kind())
                                 : std::vector<Task>{};
    emit tasksChanged();
}

QVariantList RunPanelModel::tasks() const
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(m_tasks.size()));

    for (const Task& task : m_tasks) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), task.name);
        entry.insert(QStringLiteral("kind"), buildrun::taskKindLabel(task.kind));
        entry.insert(QStringLiteral("commandLine"), task.commandLine());
        result.append(entry);
    }
    return result;
}

int RunPanelModel::errorCount() const
{
    return static_cast<int>(std::count_if(
        m_problems.cbegin(), m_problems.cend(), [](const Problem& problem) {
            return problem.severity == ProblemSeverity::Error;
        }));
}

int RunPanelModel::warningCount() const
{
    return static_cast<int>(std::count_if(
        m_problems.cbegin(), m_problems.cend(), [](const Problem& problem) {
            return problem.severity == ProblemSeverity::Warning;
        }));
}

QVariantList RunPanelModel::problems() const
{
    QVariantList result;
    result.reserve(static_cast<qsizetype>(m_problems.size()));

    for (const Problem& problem : m_problems) {
        QVariantMap entry;
        entry.insert(QStringLiteral("file"), problem.file);
        entry.insert(QStringLiteral("fileName"), QFileInfo(problem.file).fileName());
        entry.insert(QStringLiteral("line"), problem.line);
        entry.insert(QStringLiteral("column"), problem.column);
        entry.insert(QStringLiteral("severity"), static_cast<int>(problem.severity));
        entry.insert(QStringLiteral("severityLabel"),
                     ProblemParser::severityLabel(problem.severity));
        entry.insert(QStringLiteral("message"), problem.message);
        result.append(entry);
    }
    return result;
}

void RunPanelModel::appendLine(Line line)
{
    // Dropping from the front is a removal the view has to see, so it is done as
    // its own model operation rather than folded into the insert.
    if (m_lines.size() >= static_cast<size_t>(kMaxLines)) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_lines.pop_front();
        endRemoveRows();
    }

    const int row = static_cast<int>(m_lines.size());
    beginInsertRows(QModelIndex(), row, row);
    m_lines.push_back(std::move(line));
    endInsertRows();

    emit countChanged();
    emit lineAppended();
}

void RunPanelModel::appendLines(const QStringList& texts, bool isError)
{
    bool foundProblem = false;

    for (const QString& text : texts) {
        Line line;
        line.text = text;
        line.isError = isError;

        // Parsed as it streams rather than after the run, so the problem list
        // fills while the build is still going.
        if (std::optional<Problem> problem = ProblemParser::parseLine(text)) {
            line.severity = static_cast<int>(problem->severity);
            m_problems.push_back(std::move(*problem));
            foundProblem = true;
        }

        appendLine(std::move(line));
    }

    if (foundProblem) {
        emit problemsChanged();
    }
}

int RunPanelModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_lines.size());
}

QVariant RunPanelModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const Line& line = m_lines.at(static_cast<size_t>(index.row()));

    switch (role) {
    case TextRole:
        return line.text;
    case IsErrorRole:
        return line.isError;
    case SeverityRole:
        return line.severity;
    case IsCommandRole:
        return line.isCommand;
    default:
        return {};
    }
}

QHash<int, QByteArray> RunPanelModel::roleNames() const
{
    return {
        {TextRole, "lineText"},
        {IsErrorRole, "isError"},
        {SeverityRole, "severity"},
        {IsCommandRole, "isCommand"},
    };
}

void RunPanelModel::start(const Task& task)
{
    const core::Status status = m_runner.start(task, m_project.root());
    if (status) {
        return;
    }

    // Reported into the console rather than swallowed: a button that appears to
    // do nothing is indistinguishable from a broken one.
    Line line;
    line.text = status.error().message();
    line.isError = true;
    line.severity = static_cast<int>(ProblemSeverity::Error);
    appendLine(std::move(line));

    m_summary = status.error().message();
    m_lastRunFailed = true;
    emit statusChanged();
}

void RunPanelModel::runTask(int index)
{
    if (index < 0 || index >= static_cast<int>(m_tasks.size())) {
        return;
    }
    start(m_tasks.at(static_cast<size_t>(index)));
}

void RunPanelModel::runBuild()
{
    const auto it = std::find_if(m_tasks.cbegin(), m_tasks.cend(), [](const Task& task) {
        return task.kind == buildrun::TaskKind::Build;
    });
    if (it != m_tasks.cend()) {
        start(*it);
    }
}

void RunPanelModel::runRun()
{
    const auto it = std::find_if(m_tasks.cbegin(), m_tasks.cend(), [](const Task& task) {
        return task.kind == buildrun::TaskKind::Run;
    });
    if (it != m_tasks.cend()) {
        start(*it);
    }
}

bool RunPanelModel::hasBuildTask() const
{
    return std::any_of(m_tasks.cbegin(), m_tasks.cend(), [](const Task& task) {
        return task.kind == buildrun::TaskKind::Build;
    });
}

bool RunPanelModel::hasRunTask() const
{
    return std::any_of(m_tasks.cbegin(), m_tasks.cend(), [](const Task& task) {
        return task.kind == buildrun::TaskKind::Run;
    });
}

void RunPanelModel::stop()
{
    m_runner.stop();
}

void RunPanelModel::clear()
{
    beginResetModel();
    m_lines.clear();
    endResetModel();

    m_problems.clear();

    emit countChanged();
    emit problemsChanged();
}

void RunPanelModel::openProblem(int index)
{
    if (index < 0 || index >= static_cast<int>(m_problems.size())) {
        return;
    }

    const Problem& problem = m_problems.at(static_cast<size_t>(index));

    // Compilers print paths relative to wherever they were run, which is the
    // project root for every default task. An absolute path is left alone.
    const QString absolute = QFileInfo(problem.file).isAbsolute()
                                 ? problem.file
                                 : QDir(m_project.root()).filePath(problem.file);

    emit problemActivated(QDir::cleanPath(absolute), problem.line, problem.column);
}

} // namespace keys::ui
