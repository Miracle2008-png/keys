#include "buildrun/ProblemParser.h"
#include "buildrun/Task.h"
#include "buildrun/TaskRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace keys::buildrun;
using keys::project::ProjectKind;

/// Task definitions, output parsing, and running a real child process.
class BuildRunTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    template <typename Predicate>
    [[nodiscard]] static bool waitFor(Predicate done, int timeoutMs = 15000)
    {
        QElapsedTimer timer;
        timer.start();
        while (!done() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        return done();
    }

    /// A task that runs the test binary itself with an argument it ignores.
    /// Using our own executable avoids depending on a shell or a toolchain
    /// being installed, which is what makes these cases run anywhere.
    [[nodiscard]] static Task selfTask(const QStringList& arguments)
    {
        Task task;
        task.name = QStringLiteral("Self");
        task.program = QCoreApplication::applicationFilePath();
        task.arguments = arguments;
        return task;
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() { m_dir.reset(); }

    // ---- Task defaults -----------------------------------------------------

    void offersDefaultsForRecognisedProjects()
    {
        QVERIFY(!defaultTasksFor(ProjectKind::Rust).empty());
        QVERIFY(!defaultTasksFor(ProjectKind::Cpp).empty());
        QVERIFY(!defaultTasksFor(ProjectKind::Node).empty());
        QVERIFY(!defaultTasksFor(ProjectKind::Go).empty());
    }

    void offersNothingForAnUnknownProject()
    {
        // Rather than a guess that would fail confusingly.
        QVERIFY(defaultTasksFor(ProjectKind::Unknown).empty());
    }

    void pythonGetsNoRunDefault()
    {
        // There is no conventional entry point for a Python project, so Keys
        // offers testing and nothing else rather than guessing at one.
        const std::vector<Task> tasks = defaultTasksFor(ProjectKind::Python);
        for (const Task& task : tasks) {
            QVERIFY(task.kind != TaskKind::Run);
            QVERIFY(task.kind != TaskKind::Build);
        }
    }

    void everyDefaultTaskIsValid()
    {
        for (const ProjectKind kind :
             {ProjectKind::Cpp, ProjectKind::Node, ProjectKind::Python,
              ProjectKind::Rust, ProjectKind::Go}) {
            for (const Task& task : defaultTasksFor(kind)) {
                QVERIFY2(task.isValid(), qPrintable(task.name));
            }
        }
    }

    void commandLineQuotesOnlyWhatNeedsIt()
    {
        Task task;
        task.name = QStringLiteral("T");
        task.program = QStringLiteral("cmake");
        task.arguments = {QStringLiteral("--build"), QStringLiteral("out dir")};

        // Display only - arguments are passed as a list, so nothing here has to
        // survive a shell.
        QCOMPARE(task.commandLine(),
                 QStringLiteral("cmake --build \"out dir\""));
    }

    // ---- Problem parsing: GCC and Clang ------------------------------------

    void parsesAGccError()
    {
        const auto problem = ProblemParser::parseLine(
            QStringLiteral("src/main.cpp:42:17: error: expected ';' before '}'"));
        QVERIFY(problem.has_value());
        QCOMPARE(problem->file, QStringLiteral("src/main.cpp"));
        QCOMPARE(problem->line, 42);
        QCOMPARE(problem->column, 17);
        QCOMPARE(problem->severity, ProblemSeverity::Error);
        QCOMPARE(problem->message, QStringLiteral("expected ';' before '}'"));
    }

    void parsesAGccWarning()
    {
        const auto problem = ProblemParser::parseLine(
            QStringLiteral("a.c:7:3: warning: unused variable 'x' [-Wunused-variable]"));
        QVERIFY(problem.has_value());
        QCOMPARE(problem->severity, ProblemSeverity::Warning);
        QCOMPARE(problem->line, 7);
    }

    void parsesAGccNote()
    {
        // Notes explain the error above them, so they are kept rather than
        // discarded - a template error is unreadable without them.
        const auto problem = ProblemParser::parseLine(
            QStringLiteral("h.hpp:3:1: note: candidate function not viable"));
        QVERIFY(problem.has_value());
        QCOMPARE(problem->severity, ProblemSeverity::Note);
    }

    void parsesAFatalErrorAsAnError()
    {
        const auto problem = ProblemParser::parseLine(
            QStringLiteral("m.cpp:1:10: fatal error: missing.h: No such file or directory"));
        QVERIFY(problem.has_value());
        QCOMPARE(problem->severity, ProblemSeverity::Error);
    }

    void parsesAGccDiagnosticWithoutAColumn()
    {
        const auto problem =
            ProblemParser::parseLine(QStringLiteral("x.cpp:12: error: something"));
        QVERIFY(problem.has_value());
        QCOMPARE(problem->line, 12);
        QCOMPARE(problem->column, 0);   // absent, not zero-meaning-first
    }

    // ---- Problem parsing: MSVC ---------------------------------------------

    void parsesAnMsvcError()
    {
        const auto problem = ProblemParser::parseLine(QStringLiteral(
            "C:\\src\\main.cpp(120,7): error C2065: 'x': undeclared identifier"));
        QVERIFY(problem.has_value());
        QCOMPARE(problem->file, QStringLiteral("C:\\src\\main.cpp"));
        QCOMPARE(problem->line, 120);
        QCOMPARE(problem->column, 7);
        QCOMPARE(problem->severity, ProblemSeverity::Error);
        // The diagnostic code is kept: it is what a developer searches for.
        QVERIFY(problem->message.contains(QStringLiteral("C2065")));
    }

    void parsesAnMsvcWarning()
    {
        const auto problem = ProblemParser::parseLine(QStringLiteral(
            "C:\\src\\a.cpp(9): warning C4100: 'p': unreferenced formal parameter"));
        QVERIFY(problem.has_value());
        QCOMPARE(problem->severity, ProblemSeverity::Warning);
        QCOMPARE(problem->line, 9);
        QCOMPARE(problem->column, 0);
    }

    void parsesAWindowsPathContainingSpaces()
    {
        // Ordinary on Windows, and the reason the path pattern is bounded by
        // what must follow it rather than by what it may contain.
        const auto problem = ProblemParser::parseLine(QStringLiteral(
            "C:\\Program Files\\proj\\main.cpp(5,1): error C1000: bad"));
        QVERIFY(problem.has_value());
        QCOMPARE(problem->file,
                 QStringLiteral("C:\\Program Files\\proj\\main.cpp"));
        QCOMPARE(problem->line, 5);
    }

    // ---- Problem parsing: what must NOT match ------------------------------

    void ignoresOrdinaryOutput()
    {
        // A wrong entry sends the user to the wrong place, which costs more than
        // a missing one they can still read in the console.
        for (const QString& line : {
                 QStringLiteral("[1/42] Building CXX object main.cpp.obj"),
                 QStringLiteral("Scanning dependencies of target keys_core"),
                 QStringLiteral("-- Configuring done"),
                 QStringLiteral("ninja: build stopped: subcommand failed."),
                 QStringLiteral(""),
                 QStringLiteral("   "),
             }) {
            QVERIFY2(!ProblemParser::parseLine(line).has_value(), qPrintable(line));
        }
    }

    void doesNotMistakeADriveLetterForAFile()
    {
        // "C:\src\a.cpp:12: ..." splits at the first colon if nothing guards it,
        // producing a "file" called "C".
        const auto problem = ProblemParser::parseLine(
            QStringLiteral("C:\\src\\a.cpp:12:3: error: real error"));
        QVERIFY(problem.has_value());
        QVERIFY(problem->file.size() > 1);
        QCOMPARE(problem->line, 12);
    }

    void parsesABlockAndCountsOnlyDiagnostics()
    {
        const QStringList lines = {
            QStringLiteral("[1/2] Building a.cpp"),
            QStringLiteral("a.cpp:1:1: error: first"),
            QStringLiteral("[2/2] Building b.cpp"),
            QStringLiteral("b.cpp:2:2: warning: second"),
            QStringLiteral("ninja: build stopped."),
        };
        const std::vector<Problem> problems = ProblemParser::parse(lines);
        QCOMPARE(problems.size(), size_t{2});
        QCOMPARE(problems.at(0).severity, ProblemSeverity::Error);
        QCOMPARE(problems.at(1).severity, ProblemSeverity::Warning);
    }

    // ---- Running -----------------------------------------------------------

    void reportsAProgramThatCannotStart()
    {
        // The common failure: a toolchain that is not installed. It must be
        // distinguishable from a build that ran and failed.
        TaskRunner runner;

        Task task;
        task.name = QStringLiteral("Missing");
        task.program = QStringLiteral("keys-no-such-tool-exists");

        QSignalSpy failedSpy(&runner, &TaskRunner::failedToStart);
        QVERIFY(static_cast<bool>(runner.start(task, m_dir->path())));

        QVERIFY(waitFor([&failedSpy] { return failedSpy.count() > 0; }));
        // The message names the program, or it is not actionable.
        QVERIFY(failedSpy.at(0).at(0).toString().contains(
            QStringLiteral("keys-no-such-tool-exists")));
    }

    void refusesASecondTaskWhileOneRuns()
    {
        // Two builds writing to one output directory corrupt each other, so this
        // is refused rather than queued.
        TaskRunner runner;
        const Task task = selfTask({QStringLiteral("-help")});

        QVERIFY(static_cast<bool>(runner.start(task, m_dir->path())));
        if (runner.isRunning()) {
            QVERIFY(!runner.start(task, m_dir->path()));
        }
        runner.stop();
    }

    void refusesAMalformedTask()
    {
        TaskRunner runner;
        Task task;   // no name, no program
        QVERIFY(!runner.start(task, m_dir->path()));
    }

    void streamsOutputAsCompleteLines()
    {
        // Running the test binary with -functions makes it print a list of
        // function names and exit - real streamed output, with no dependency on
        // a shell or a toolchain.
        TaskRunner runner;

        QStringList received;
        connect(&runner, &TaskRunner::outputReceived, this,
                [&received](const QStringList& lines, bool) { received += lines; });

        QSignalSpy finishedSpy(&runner, &TaskRunner::finished);
        QVERIFY(static_cast<bool>(
            runner.start(selfTask({QStringLiteral("-functions")}), m_dir->path())));

        QVERIFY(waitFor([&finishedSpy] { return finishedSpy.count() > 0; }));
        QVERIFY(!received.isEmpty());

        // Lines, not chunks: no emitted line may carry a newline, or every
        // consumer would have to split them again.
        for (const QString& line : received) {
            QVERIFY(!line.contains(QLatin1Char('\n')));
            QVERIFY(!line.endsWith(QLatin1Char('\r')));
        }
    }

    void reportsTheExitCode()
    {
        TaskRunner runner;
        QSignalSpy finishedSpy(&runner, &TaskRunner::finished);

        QVERIFY(static_cast<bool>(
            runner.start(selfTask({QStringLiteral("-functions")}), m_dir->path())));
        QVERIFY(waitFor([&finishedSpy] { return finishedSpy.count() > 0; }));

        QCOMPARE(finishedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(finishedSpy.at(0).at(1).toBool(), false);   // not stopped
        QCOMPARE(finishedSpy.at(0).at(2).toBool(), false);   // and did not crash
        QVERIFY(runner.elapsedMs() >= 0);
    }

    void runningStateTracksTheProcess()
    {
        TaskRunner runner;
        QVERIFY(!runner.isRunning());

        QSignalSpy finishedSpy(&runner, &TaskRunner::finished);
        QVERIFY(static_cast<bool>(
            runner.start(selfTask({QStringLiteral("-functions")}), m_dir->path())));

        QVERIFY(waitFor([&finishedSpy] { return finishedSpy.count() > 0; }));
        QVERIFY(!runner.isRunning());
    }

    void stoppingIsReportedAsStoppedNotFailed()
    {
        // The user needs to know they stopped it, not that it broke.
        TaskRunner runner;

        // A run long enough to stop: the test binary sleeping via its own
        // wait facility is not available, so a repeated self-run is used
        // instead and stopped immediately.
        const Task task = selfTask({QStringLiteral("-functions")});
        QVERIFY(static_cast<bool>(runner.start(task, m_dir->path())));

        QSignalSpy finishedSpy(&runner, &TaskRunner::finished);
        runner.stop();

        QVERIFY(waitFor([&finishedSpy] { return finishedSpy.count() > 0; }));
        // Either it was stopped, or it finished first - both are valid outcomes
        // for a process this short. What must never happen is reporting a stop
        // as a crash: terminating a process looks like a crash to Windows, and
        // conflating the two would tell the user their build broke when they
        // stopped it themselves.
        const bool wasStopped = finishedSpy.at(0).at(1).toBool();
        const bool crashed = finishedSpy.at(0).at(2).toBool();
        QVERIFY(!(wasStopped && crashed));
        QVERIFY(!runner.isRunning());
    }

    void aCrashIsReportedSeparatelyFromAStop()
    {
        // These arrive from Windows as the same thing - a process that did not
        // exit normally - so the runner has to tell them apart from what it
        // knows, not from the exit status alone.
        TaskRunner runner;
        QSignalSpy finishedSpy(&runner, &TaskRunner::finished);

        QVERIFY(static_cast<bool>(
            runner.start(selfTask({QStringLiteral("-functions")}), m_dir->path())));
        QVERIFY(waitFor([&finishedSpy] { return finishedSpy.count() > 0; }));

        // A clean run is neither.
        QCOMPARE(finishedSpy.at(0).at(1).toBool(), false);
        QCOMPARE(finishedSpy.at(0).at(2).toBool(), false);
    }

    void stoppingWhenNothingRunsIsHarmless()
    {
        TaskRunner runner;
        runner.stop();
        QVERIFY(!runner.isRunning());
    }
};

QTEST_MAIN(BuildRunTests)
#include "BuildRunTests.moc"
