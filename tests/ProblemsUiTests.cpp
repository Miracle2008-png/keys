#include "langsvc/LanguageServiceManager.h"
#include "project/Project.h"
#include "ui/ProblemsModel.h"

#include <QSignalSpy>
#include <QTest>

#include <memory>

using keys::langsvc::Diagnostic;
using keys::langsvc::DiagnosticSeverity;
using keys::langsvc::LanguageServiceManager;
using keys::project::Project;
using keys::ui::ProblemsModel;

/// The problems panel's model.
///
/// What this pins down is the accumulation. A language server publishes
/// diagnostics per file and clears a file by publishing an empty list for it,
/// so a model that treated each publication as the whole truth would show only
/// the last file analysed - which looks exactly like a working panel until a
/// second file is opened.
class ProblemsUiTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<LanguageServiceManager> m_services;
    std::unique_ptr<Project> m_project;
    std::unique_ptr<ProblemsModel> m_model;

    [[nodiscard]] static Diagnostic problem(int line, DiagnosticSeverity severity,
                                            const QString& message)
    {
        Diagnostic diagnostic;
        diagnostic.range.start.line = line;
        diagnostic.range.start.character = 4;
        diagnostic.severity = severity;
        diagnostic.message = message;
        diagnostic.source = QStringLiteral("clangd");
        return diagnostic;
    }

    void publish(const QString& path, const std::vector<Diagnostic>& diagnostics)
    {
        emit m_services->diagnosticsPublished(path, diagnostics);
    }

    [[nodiscard]] QVariant roleOf(int row, const char* name) const
    {
        const QHash<int, QByteArray> names = m_model->roleNames();
        for (auto it = names.cbegin(); it != names.cend(); ++it) {
            if (it.value() == name) {
                return m_model->data(m_model->index(row, 0), it.key());
            }
        }
        return {};
    }

private slots:
    void init()
    {
        m_services = std::make_unique<LanguageServiceManager>();
        m_project = std::make_unique<Project>();
        m_model = std::make_unique<ProblemsModel>(*m_services, *m_project);
    }

    void cleanup()
    {
        m_model.reset();
        m_project.reset();
        m_services.reset();
    }

    // ---- Before anything has been analysed --------------------------------

    void startsEmptyAndSaysNothingHasBeenAnalysed()
    {
        // "No problems" and "nothing analysed yet" are very different things to
        // show someone who has just opened a project.
        QCOMPARE(m_model->rowCount(), 0);
        QCOMPARE(m_model->errorCount(), 0);
        QVERIFY(!m_model->hasAnalysed());
    }

    // ---- Accumulation -----------------------------------------------------

    void keepsDiagnosticsFromEveryFile()
    {
        publish(QStringLiteral("/p/a.cpp"),
                {problem(1, DiagnosticSeverity::Error, QStringLiteral("bad a"))});
        publish(QStringLiteral("/p/b.cpp"),
                {problem(2, DiagnosticSeverity::Error, QStringLiteral("bad b"))});

        // The regression: taking the latest publication as the whole truth
        // would leave one file here.
        QCOMPARE(m_model->fileCount(), 2);
        QCOMPARE(m_model->errorCount(), 2);
        QCOMPARE(m_model->rowCount(), 4);   // two headings, two problems
    }

    void republishingAFileReplacesOnlyThatFile()
    {
        publish(QStringLiteral("/p/a.cpp"),
                {problem(1, DiagnosticSeverity::Error, QStringLiteral("first"))});
        publish(QStringLiteral("/p/b.cpp"),
                {problem(2, DiagnosticSeverity::Error, QStringLiteral("other"))});

        publish(QStringLiteral("/p/a.cpp"),
                {problem(5, DiagnosticSeverity::Warning, QStringLiteral("second"))});

        QCOMPARE(m_model->fileCount(), 2);
        QCOMPARE(m_model->errorCount(), 1);     // only b's
        QCOMPARE(m_model->warningCount(), 1);   // a's replacement
    }

    void anEmptyPublicationClearsThatFile()
    {
        // How a server says "this file is clean now". Ignoring it would leave
        // the panel showing problems the user has already fixed.
        publish(QStringLiteral("/p/a.cpp"),
                {problem(1, DiagnosticSeverity::Error, QStringLiteral("bad"))});
        QCOMPARE(m_model->fileCount(), 1);

        publish(QStringLiteral("/p/a.cpp"), {});
        QCOMPARE(m_model->fileCount(), 0);
        QCOMPARE(m_model->errorCount(), 0);
        QVERIFY(m_model->hasAnalysed());   // it has been analysed, and is clean
    }

    // ---- Ordering ---------------------------------------------------------

    void worstProblemsComeFirstWithinAFile()
    {
        publish(QStringLiteral("/p/a.cpp"), {
            problem(50, DiagnosticSeverity::Hint, QStringLiteral("hint")),
            problem(10, DiagnosticSeverity::Error, QStringLiteral("error")),
            problem(30, DiagnosticSeverity::Warning, QStringLiteral("warning")),
        });

        // An error twenty lines down matters more than a hint on line one.
        QCOMPARE(roleOf(1, "severity").toInt(),
                 static_cast<int>(ProblemsModel::ErrorSeverity));
        QCOMPARE(roleOf(2, "severity").toInt(),
                 static_cast<int>(ProblemsModel::WarningSeverity));
    }

    void filesAreOrderedStablyRatherThanByArrival()
    {
        // Servers finish in whatever order they finish; the list must not
        // reshuffle underneath someone reading it.
        publish(QStringLiteral("/p/zulu.cpp"),
                {problem(1, DiagnosticSeverity::Error, QStringLiteral("z"))});
        publish(QStringLiteral("/p/alpha.cpp"),
                {problem(1, DiagnosticSeverity::Error, QStringLiteral("a"))});

        QCOMPARE(roleOf(0, "fileName").toString(), QStringLiteral("alpha.cpp"));
    }

    void aFileRowReportsItsWorstSeverity()
    {
        // So a folded file still says how bad it is.
        publish(QStringLiteral("/p/a.cpp"), {
            problem(1, DiagnosticSeverity::Warning, QStringLiteral("w")),
            problem(2, DiagnosticSeverity::Error, QStringLiteral("e")),
        });

        QCOMPARE(roleOf(0, "worstSeverity").toInt(),
                 static_cast<int>(ProblemsModel::ErrorSeverity));
    }

    // ---- Positions --------------------------------------------------------

    void positionsAreOneBasedForDisplay()
    {
        // LSP is zero-based and every editor's gutter is one-based; getting
        // this wrong lands the caret a line early on every jump.
        publish(QStringLiteral("/p/a.cpp"),
                {problem(9, DiagnosticSeverity::Error, QStringLiteral("bad"))});

        QCOMPARE(roleOf(1, "line").toInt(), 10);
        QCOMPARE(roleOf(1, "column").toInt(), 5);
    }

    void activatingAProblemReportsAOneBasedLocation()
    {
        publish(QStringLiteral("/p/a.cpp"),
                {problem(9, DiagnosticSeverity::Error, QStringLiteral("bad"))});

        QSignalSpy activated(m_model.get(), &ProblemsModel::problemActivated);
        m_model->activate(1);

        QCOMPARE(activated.count(), 1);
        const QList<QVariant> arguments = activated.takeFirst();
        QCOMPARE(arguments.at(0).toString(), QStringLiteral("/p/a.cpp"));
        QCOMPARE(arguments.at(1).toInt(), 10);
        QCOMPARE(arguments.at(2).toInt(), 5);
    }

    void activatingAFileRowCollapsesItRatherThanOpeningIt()
    {
        publish(QStringLiteral("/p/a.cpp"), {
            problem(1, DiagnosticSeverity::Error, QStringLiteral("one")),
            problem(2, DiagnosticSeverity::Error, QStringLiteral("two")),
        });
        QCOMPARE(m_model->rowCount(), 3);

        const QSignalSpy activated(m_model.get(), &ProblemsModel::problemActivated);

        m_model->activate(0);
        QCOMPARE(m_model->rowCount(), 1);
        QCOMPARE(activated.count(), 0);

        m_model->activate(0);
        QCOMPARE(m_model->rowCount(), 3);
    }

    // ---- Filtering --------------------------------------------------------

    void theFilterHidesRowsButNotTheCounts()
    {
        // A filter that also changed the counts would make it impossible to
        // tell that errors were being hidden.
        publish(QStringLiteral("/p/a.cpp"), {
            problem(1, DiagnosticSeverity::Error, QStringLiteral("e")),
            problem(2, DiagnosticSeverity::Hint, QStringLiteral("h")),
        });
        QCOMPARE(m_model->rowCount(), 3);

        m_model->setErrorsAndWarningsOnly(true);
        QCOMPARE(m_model->rowCount(), 2);       // the heading and the error
        QCOMPARE(m_model->errorCount(), 1);
        QCOMPARE(m_model->infoCount(), 1);      // still counted
    }

    void aFileWithOnlyFilteredProblemsDisappearsEntirely()
    {
        // Rather than leaving a heading with nothing under it.
        publish(QStringLiteral("/p/a.cpp"),
                {problem(1, DiagnosticSeverity::Hint, QStringLiteral("h"))});
        QCOMPARE(m_model->fileCount(), 1);

        m_model->setErrorsAndWarningsOnly(true);
        QCOMPARE(m_model->fileCount(), 0);
    }

    // ---- Clearing ---------------------------------------------------------

    void clearingForgetsEverything()
    {
        publish(QStringLiteral("/p/a.cpp"),
                {problem(1, DiagnosticSeverity::Error, QStringLiteral("bad"))});

        m_model->clear();
        QCOMPARE(m_model->rowCount(), 0);
        QCOMPARE(m_model->errorCount(), 0);
        QVERIFY(!m_model->hasAnalysed());
    }

    void actingOnAnAbsentRowIsSafe()
    {
        const QSignalSpy activated(m_model.get(), &ProblemsModel::problemActivated);
        m_model->activate(-1);
        m_model->activate(0);
        m_model->activate(500);
        QCOMPARE(activated.count(), 0);
    }

    void exposesTheRolesThePanelBindsTo()
    {
        const QHash<int, QByteArray> names = m_model->roleNames();
        for (const char* role : {"kind", "path", "fileName", "directory",
                                 "problemCount", "collapsed", "severity",
                                 "message", "source", "line", "column",
                                 "worstSeverity"}) {
            QVERIFY2(names.values().contains(QByteArray(role)), role);
        }
    }
};

QTEST_MAIN(ProblemsUiTests)
#include "ProblemsUiTests.moc"
