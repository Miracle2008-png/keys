#include "core/TaskScheduler.h"
#include "project/Project.h"
#include "search/FileIndex.h"
#include "search/TextSearch.h"
#include "ui/SearchModel.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

#include <memory>

using keys::core::TaskScheduler;
using keys::project::Project;
using keys::search::FileIndex;
using keys::search::TextSearch;
using keys::ui::SearchModel;

/// The search panel's model, against a real project on disk.
///
/// What this pins down is the flattening: a search returns matches, the panel
/// draws a file heading followed by its matches, and the two have to stay
/// consistent as batches arrive, as files collapse, and as the query changes.
class SearchUiTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<TaskScheduler> m_scheduler;
    std::unique_ptr<Project> m_project;
    std::unique_ptr<FileIndex> m_index;
    std::unique_ptr<TextSearch> m_search;
    std::unique_ptr<SearchModel> m_model;

    void write(const QString& relative, const QString& contents) const
    {
        const QString absolute = QDir(m_dir->path()).filePath(relative);
        QDir().mkpath(QFileInfo(absolute).path());

        QFile file(absolute);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file);
        stream << contents;
    }

    /// Runs the query and waits for the search to finish, rather than sleeping
    /// for a duration that would be either flaky or slow.
    void searchFor(const QString& query)
    {
        QSignalSpy finished(m_search.get(), &TextSearch::finished);
        m_model->setQuery(query);
        m_model->searchNow();
        QVERIFY(finished.wait(5000));

        // The model rebuilds on `finished`; that slot may run after the spy's.
        QCoreApplication::processEvents();
    }

    /// The roles a row exposes, by name, so the tests read as the panel does.
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
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());

        m_scheduler = std::make_unique<TaskScheduler>();
        m_project = std::make_unique<Project>();
        m_index = std::make_unique<FileIndex>(*m_project, *m_scheduler);

        m_search = std::make_unique<TextSearch>(*m_project, *m_index, *m_scheduler);
        m_model = std::make_unique<SearchModel>(*m_search);
    }

    void cleanup()
    {
        m_model.reset();
        m_search.reset();
        m_index.reset();
        m_project.reset();
        m_scheduler.reset();
        m_dir.reset();
    }

    /// Opens the project and waits for the index, which every search needs.
    void openProject()
    {
        QVERIFY(m_project->open(m_dir->path()));

        QSignalSpy indexed(m_index.get(), &FileIndex::changed);
        m_index->rebuild();
        if (m_index->files().empty()) {
            QVERIFY(indexed.wait(5000));
        }
    }

    // ---- Nothing searched for yet -----------------------------------------

    void startsEmptyAndSaysNothing()
    {
        // "No matches" and "you have not searched" are different states, and a
        // panel that shows the first before a query is typed is wrong.
        QCOMPARE(m_model->rowCount(), 0);
        QVERIFY(!m_model->hasSearched());
        QVERIFY(!m_model->isSearching());
        QCOMPARE(m_model->matchCount(), 0);
    }

    // ---- The flattening ---------------------------------------------------

    void groupsMatchesUnderAFileHeading()
    {
        write(QStringLiteral("alpha.txt"), QStringLiteral("needle\nhay\nneedle\n"));
        openProject();
        searchFor(QStringLiteral("needle"));

        // One heading plus two matches.
        QCOMPARE(m_model->rowCount(), 3);
        QCOMPARE(m_model->fileCount(), 1);
        QCOMPARE(m_model->matchCount(), 2);

        QCOMPARE(roleOf(0, "kind").toInt(), static_cast<int>(SearchModel::FileRow));
        QCOMPARE(roleOf(0, "fileName").toString(), QStringLiteral("alpha.txt"));
        QCOMPARE(roleOf(0, "matchCount").toInt(), 2);

        QCOMPARE(roleOf(1, "kind").toInt(), static_cast<int>(SearchModel::MatchRow));
        QCOMPARE(roleOf(2, "kind").toInt(), static_cast<int>(SearchModel::MatchRow));
    }

    void linesAreOneBasedForDisplay()
    {
        // Stored zero-based to match the editor and LSP; shown one-based
        // because that is what every editor's gutter says.
        write(QStringLiteral("alpha.txt"), QStringLiteral("first\nsecond needle\n"));
        openProject();
        searchFor(QStringLiteral("needle"));

        QCOMPARE(roleOf(1, "line").toInt(), 2);
        QCOMPARE(roleOf(1, "column").toInt(), 8);
    }

    void matchSpanIsReportedForHighlighting()
    {
        write(QStringLiteral("alpha.txt"), QStringLiteral("a needle here\n"));
        openProject();
        searchFor(QStringLiteral("needle"));

        QCOMPARE(roleOf(1, "matchStart").toInt(), 2);
        QCOMPARE(roleOf(1, "matchLength").toInt(), 6);
        QCOMPARE(roleOf(1, "lineText").toString(), QStringLiteral("a needle here"));
    }

    void filesAreSortedSoTheListDoesNotReorderAsResultsArrive()
    {
        // Workers finish in whatever order they finish. Without a sort the list
        // would shuffle under the user while a search is still running.
        write(QStringLiteral("zulu.txt"), QStringLiteral("needle\n"));
        write(QStringLiteral("alpha.txt"), QStringLiteral("needle\n"));
        write(QStringLiteral("mike.txt"), QStringLiteral("needle\n"));
        openProject();
        searchFor(QStringLiteral("needle"));

        QCOMPARE(m_model->fileCount(), 3);
        QCOMPARE(roleOf(0, "fileName").toString(), QStringLiteral("alpha.txt"));
        QCOMPARE(roleOf(2, "fileName").toString(), QStringLiteral("mike.txt"));
        QCOMPARE(roleOf(4, "fileName").toString(), QStringLiteral("zulu.txt"));
    }

    // ---- Collapsing -------------------------------------------------------

    void activatingAFileRowCollapsesItRatherThanOpeningIt()
    {
        write(QStringLiteral("alpha.txt"), QStringLiteral("needle\nneedle\n"));
        openProject();
        searchFor(QStringLiteral("needle"));
        QCOMPARE(m_model->rowCount(), 3);

        const QSignalSpy activated(m_model.get(), &SearchModel::matchActivated);

        m_model->activate(0);
        QCOMPARE(m_model->rowCount(), 1);          // the heading alone
        QVERIFY(roleOf(0, "collapsed").toBool());
        QCOMPARE(activated.count(), 0);            // a heading opens nothing

        m_model->activate(0);
        QCOMPARE(m_model->rowCount(), 3);
        QVERIFY(!roleOf(0, "collapsed").toBool());
    }

    void collapsedFilesStayCollapsedWhenResultsArrive()
    {
        // Batches keep landing while the user is reading. A rebuild that reset
        // the collapse would spring open a file they had just folded away.
        write(QStringLiteral("alpha.txt"), QStringLiteral("needle\nneedle\n"));
        openProject();
        searchFor(QStringLiteral("needle"));

        m_model->activate(0);
        QVERIFY(roleOf(0, "collapsed").toBool());

        searchFor(QStringLiteral("needle"));
        QVERIFY(roleOf(0, "collapsed").toBool());
    }

    // ---- Activation -------------------------------------------------------

    void activatingAMatchReportsAOneBasedLocation()
    {
        // AppController::openFileAt takes one-based line and column; handing it
        // the stored zero-based pair would land the caret a line and a column
        // early, which is the kind of off-by-one nobody reports and everybody
        // notices.
        write(QStringLiteral("alpha.txt"), QStringLiteral("one\ntwo needle\n"));
        openProject();
        searchFor(QStringLiteral("needle"));

        QSignalSpy activated(m_model.get(), &SearchModel::matchActivated);
        m_model->activate(1);

        QCOMPARE(activated.count(), 1);
        const QList<QVariant> arguments = activated.takeFirst();
        QCOMPARE(arguments.at(0).toString(), QStringLiteral("alpha.txt"));
        QCOMPARE(arguments.at(1).toInt(), 2);
        QCOMPARE(arguments.at(2).toInt(), 5);
    }

    void activatingOutOfRangeIsIgnored()
    {
        // The view can ask for a row that a batch has just removed.
        const QSignalSpy activated(m_model.get(), &SearchModel::matchActivated);
        m_model->activate(-1);
        m_model->activate(0);
        m_model->activate(9999);
        QCOMPARE(activated.count(), 0);
    }

    // ---- Options ----------------------------------------------------------

    void caseSensitivityIsHonoured()
    {
        write(QStringLiteral("alpha.txt"), QStringLiteral("Needle\nneedle\n"));
        openProject();

        searchFor(QStringLiteral("needle"));
        QCOMPARE(m_model->matchCount(), 2);

        m_model->setCaseSensitive(true);
        searchFor(QStringLiteral("needle"));
        QCOMPARE(m_model->matchCount(), 1);
    }

    void anUncompilableExpressionIsReportedRatherThanShownAsNoMatches()
    {
        // The regression this guards: TextSearch treats a broken pattern as the
        // user still typing and stays silent, which in a panel would read as
        // "your pattern is fine and matched nothing".
        write(QStringLiteral("alpha.txt"), QStringLiteral("needle\n"));
        openProject();

        m_model->setRegularExpression(true);
        m_model->setQuery(QStringLiteral("needle("));
        m_model->searchNow();

        QVERIFY(!m_model->patternError().isEmpty());
        QCOMPARE(m_model->rowCount(), 0);
        QVERIFY(m_model->hasSearched());
    }

    void fixingTheExpressionClearsTheError()
    {
        write(QStringLiteral("alpha.txt"), QStringLiteral("needle\n"));
        openProject();

        m_model->setRegularExpression(true);
        m_model->setQuery(QStringLiteral("needle("));
        m_model->searchNow();
        QVERIFY(!m_model->patternError().isEmpty());

        searchFor(QStringLiteral("need.e"));
        QVERIFY(m_model->patternError().isEmpty());
        QCOMPARE(m_model->matchCount(), 1);
    }

    void excludeGlobsRemoveFiles()
    {
        write(QStringLiteral("keep.txt"), QStringLiteral("needle\n"));
        write(QStringLiteral("build/drop.txt"), QStringLiteral("needle\n"));
        openProject();

        searchFor(QStringLiteral("needle"));
        const int all = m_model->fileCount();
        QVERIFY(all >= 1);

        m_model->setExcludeGlobs(QStringLiteral("build/**"));
        searchFor(QStringLiteral("needle"));

        for (int row = 0; row < m_model->rowCount(); ++row) {
            QVERIFY(!roleOf(row, "path").toString().startsWith(QLatin1String("build/")));
        }
    }

    // ---- Clearing ---------------------------------------------------------

    void clearingEmptiesTheListAndForgetsThatASearchRan()
    {
        write(QStringLiteral("alpha.txt"), QStringLiteral("needle\n"));
        openProject();
        searchFor(QStringLiteral("needle"));
        QVERIFY(m_model->rowCount() > 0);

        m_model->clear();
        QCOMPARE(m_model->rowCount(), 0);
        QVERIFY(!m_model->hasSearched());
    }

    void emptyingTheQueryClearsTheResults()
    {
        // Otherwise the panel keeps showing matches for a query the field no
        // longer contains.
        write(QStringLiteral("alpha.txt"), QStringLiteral("needle\n"));
        openProject();
        searchFor(QStringLiteral("needle"));
        QVERIFY(m_model->rowCount() > 0);

        m_model->setQuery(QString());
        QCOMPARE(m_model->rowCount(), 0);
    }
};

QTEST_MAIN(SearchUiTests)
#include "SearchUiTests.moc"
