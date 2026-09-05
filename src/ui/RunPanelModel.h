#pragma once

#include "buildrun/ProblemParser.h"
#include "buildrun/Task.h"
#include "buildrun/TaskRunner.h"
#include "project/Project.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>

#include <deque>
#include <vector>

namespace keys::ui {

/// The build console and its problem list.
///
/// **The console is a list, not a text document.** Output arrives as lines and
/// is only ever appended, so a flat model over a bounded deque lets the view
/// render the visible rows and nothing else — a fifty-thousand-line build costs
/// the same to display as a ten-line one. A QML TextArea holding the whole log
/// would re-lay-out the entire document on every read.
///
/// **Bounded.** A build that loops printing errors must not exhaust memory. The
/// oldest lines are dropped past a cap; the problem list is what the user
/// actually navigates, and it is kept whole.
class RunPanelModel : public QAbstractListModel {
    Q_OBJECT
    // Named Runner, not RunPanel: RunPanel.qml already claims that name in this
    // module, and a C++ type registering the same one makes the *file* type
    // uncreatable - the QML component silently loses to the singleton, and the
    // whole view fails to load with only "Element is not creatable".
    QML_NAMED_ELEMENT(Runner)
    QML_SINGLETON

    Q_PROPERTY(int count READ count NOTIFY countChanged)

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString currentTaskName READ currentTaskName NOTIFY statusChanged)

    /// The tasks available for the open project, as {name, kind} maps ready for
    /// a QML repeater.
    Q_PROPERTY(QVariantList tasks READ tasks NOTIFY tasksChanged)

    /// A one-line summary of the last run: what it was, how it ended, how long
    /// it took. Empty before anything has run.
    Q_PROPERTY(QString summary READ summary NOTIFY statusChanged)
    Q_PROPERTY(bool lastRunFailed READ lastRunFailed NOTIFY statusChanged)

    Q_PROPERTY(int errorCount READ errorCount NOTIFY problemsChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY problemsChanged)

    /// The problems, as maps for the QML list. Exposed as a property rather than
    /// a second model: there are few of them and they are replaced wholesale per
    /// run, so a list model would be machinery for nothing.
    Q_PROPERTY(QVariantList problems READ problems NOTIFY problemsChanged)

public:
    enum Roles {
        TextRole = Qt::UserRole + 1,
        IsErrorRole,        ///< arrived on stderr
        SeverityRole,       ///< -1 when the line is not a diagnostic
        IsCommandRole,      ///< the echoed command line that starts a run
    };

    RunPanelModel(buildrun::TaskRunner& runner, project::Project& project,
                  QObject* parent = nullptr);

    static void setInstance(RunPanelModel* instance);
    static RunPanelModel* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return static_cast<int>(m_lines.size()); }
    [[nodiscard]] bool isRunning() const { return m_runner.isRunning(); }
    [[nodiscard]] QString currentTaskName() const;

    [[nodiscard]] QVariantList tasks() const;
    [[nodiscard]] QString summary() const { return m_summary; }
    [[nodiscard]] bool lastRunFailed() const { return m_lastRunFailed; }

    [[nodiscard]] int errorCount() const;
    [[nodiscard]] int warningCount() const;
    [[nodiscard]] QVariantList problems() const;

    /// Runs the task at `index` in tasks(). Out of range is ignored rather than
    /// treated as an error: the list can change under a click.
    Q_INVOKABLE void runTask(int index);

    /// Runs the first task of a kind, for the toolbar's Build and Run buttons.
    /// Does nothing when the project has no such task.
    Q_INVOKABLE void runBuild();
    Q_INVOKABLE void runRun();

    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();

    /// Whether a kind exists, so a button can be hidden rather than shown
    /// disabled for a project that will never have it.
    Q_INVOKABLE bool hasBuildTask() const;
    Q_INVOKABLE bool hasRunTask() const;

    /// Asks the workbench to open a problem's file. Absolute, resolved against
    /// the project root for the relative paths compilers usually print.
    Q_INVOKABLE void openProblem(int index);

signals:
    void countChanged();
    void runningChanged();
    void statusChanged();
    void tasksChanged();
    void problemsChanged();

    /// A line was appended, so the view can follow the tail.
    void lineAppended();

    /// A problem was chosen. The panel does not open files itself.
    void problemActivated(const QString& absolutePath, int line, int column);

private:
    struct Line {
        QString text;
        bool isError = false;
        bool isCommand = false;
        int severity = -1;   ///< ProblemSeverity, or -1 when not a diagnostic
    };

    void appendLine(Line line);
    void appendLines(const QStringList& texts, bool isError);

    /// Refreshes the task list when the project changes kind or closes.
    void reloadTasks();

    /// Starts `task`, reporting a refusal into the console rather than
    /// silently - a button that does nothing is indistinguishable from a broken
    /// one.
    void start(const buildrun::Task& task);

    buildrun::TaskRunner& m_runner;
    project::Project& m_project;

    std::vector<buildrun::Task> m_tasks;

    /// A deque so dropping the oldest line is O(1); the cap is what keeps a
    /// runaway build from exhausting memory.
    std::deque<Line> m_lines;
    std::vector<buildrun::Problem> m_problems;

    QString m_summary;
    bool m_lastRunFailed = false;

    static constexpr int kMaxLines = 20000;
};

} // namespace keys::ui
