#include "core/TaskScheduler.h"
#include "filesystem/FileSystem.h"
#include "project/Project.h"
#include "search/FileIndex.h"
#include "search/FuzzyMatch.h"
#include "search/TextSearch.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace keys::search;
using keys::core::TaskScheduler;
using keys::fs::FileSystem;
using keys::project::Project;

class SearchTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Project> m_project;
    std::unique_ptr<TaskScheduler> m_scheduler;
    std::unique_ptr<FileIndex> m_index;

    [[nodiscard]] QString path(const QString& relative) const
    {
        return QDir(m_dir->path()).filePath(relative);
    }

    void writeFile(const QString& relative, const QString& body) const
    {
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(path(relative), body)));
    }

    /// Indexing and searching are asynchronous, so tests wait for the work to
    /// settle rather than sleeping for an arbitrary interval.
    template <typename Predicate>
    [[nodiscard]] bool waitFor(Predicate done, int timeoutMs = 5000) const
    {
        QElapsedTimer timer;
        timer.start();
        while (!done() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        return done();
    }

    [[nodiscard]] bool waitForIndex() const
    {
        return waitFor([this] { return !m_index->isBuilding(); });
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());

        m_project = std::make_unique<Project>();
        m_scheduler = std::make_unique<TaskScheduler>();
        m_index = std::make_unique<FileIndex>(*m_project, *m_scheduler);
    }

    void cleanup()
    {
        m_index.reset();
        m_scheduler.reset();
        m_project.reset();
        m_dir.reset();
    }

    // ---- Fuzzy matching: basics -------------------------------------------

    void emptyQueryMatchesEverything()
    {
        // An unfiltered quick open shows the whole list rather than nothing.
        const FuzzyResult result = FuzzyMatch::match(QString(), QStringLiteral("anything"));
        QVERIFY(result.matched());
    }

    void matchesASubsequence()
    {
        // Every query character must appear in order, not necessarily adjacent.
        QVERIFY(FuzzyMatch::match(QStringLiteral("fzm"),
                                  QStringLiteral("FuzzyMatch.h")).matched());
        QVERIFY(FuzzyMatch::match(QStringLiteral("abc"),
                                  QStringLiteral("a_b_c")).matched());
    }

    void rejectsWhenACharacterIsMissing()
    {
        QVERIFY(!FuzzyMatch::match(QStringLiteral("xyz"),
                                   QStringLiteral("FuzzyMatch.h")).matched());
    }

    void rejectsWhenOrderIsWrong()
    {
        QVERIFY(!FuzzyMatch::match(QStringLiteral("cba"),
                                   QStringLiteral("abc")).matched());
    }

    void matchingIsCaseInsensitive()
    {
        QVERIFY(FuzzyMatch::match(QStringLiteral("FM"),
                                  QStringLiteral("fuzzy_match")).matched());
    }

    void reportsMatchedPositions()
    {
        // The UI highlights these, so they have to be the real indices.
        const FuzzyResult result =
            FuzzyMatch::match(QStringLiteral("abc"), QStringLiteral("axbxc"));
        QVERIFY(result.matched());
        QCOMPARE(result.positions, (std::vector<int>{0, 2, 4}));
    }

    // ---- Fuzzy matching: ranking ------------------------------------------

    void prefersAPrefixOverAScatteredMatch()
    {
        const int prefix = FuzzyMatch::match(QStringLiteral("fuz"),
                                             QStringLiteral("fuzzy.cpp")).score;
        const int scattered = FuzzyMatch::match(QStringLiteral("fuz"),
                                                QStringLiteral("far_up_zone.cpp")).score;
        QVERIFY2(prefix > scattered, "a prefix must outrank a scattered match");
    }

    void prefersConsecutiveCharacters()
    {
        const int consecutive = FuzzyMatch::match(QStringLiteral("abc"),
                                                  QStringLiteral("abcdef")).score;
        const int spread = FuzzyMatch::match(QStringLiteral("abc"),
                                             QStringLiteral("axxbxxcxx")).score;
        QVERIFY(consecutive > spread);
    }

    void prefersWordBoundaries()
    {
        // This is how people abbreviate: initials of the humps.
        const int boundaries = FuzzyMatch::match(QStringLiteral("tbm"),
                                                 QStringLiteral("TabBarModel")).score;
        const int middle = FuzzyMatch::match(QStringLiteral("tbm"),
                                             QStringLiteral("catbomb")).score;
        QVERIFY2(boundaries > middle, "camelCase humps must outrank mid-word letters");
    }

    void prefersAnEarlyMatch()
    {
        const int early = FuzzyMatch::match(QStringLiteral("x"),
                                            QStringLiteral("xaaaaaaaa")).score;
        const int late = FuzzyMatch::match(QStringLiteral("x"),
                                           QStringLiteral("aaaaaaaax")).score;
        QVERIFY(early > late);
    }

    void everyMatchScoresAtLeastOne()
    {
        // A poor match must still sort above a non-match, never below it.
        const FuzzyResult poor =
            FuzzyMatch::match(QStringLiteral("z"), QString(200, QLatin1Char('a'))
                                                        + QStringLiteral("z"));
        QVERIFY(poor.score >= 1);
    }

    // ---- Fuzzy matching: paths --------------------------------------------

    void aFileNameMatchBeatsADirectoryMatch()
    {
        // The user is naming a file, not a folder.
        const int onFile = FuzzyMatch::matchPath(QStringLiteral("main"),
                                                 QStringLiteral("src/main.cpp")).score;
        const int onDirectory =
            FuzzyMatch::matchPath(QStringLiteral("main"),
                                  QStringLiteral("src/main/util/helper.cpp")).score;
        QVERIFY2(onFile > onDirectory,
                 "a file-name match must outrank a directory-name match");
    }

    void positionsAreReportedAgainstTheWholePath()
    {
        // The UI highlights against the string it displays.
        const FuzzyResult result =
            FuzzyMatch::matchPath(QStringLiteral("main"), QStringLiteral("src/main.cpp"));
        QVERIFY(result.matched());
        QCOMPARE(result.positions.front(), 4);
    }

    void aQueryCanSpanADirectoryBoundary()
    {
        QVERIFY(FuzzyMatch::matchPath(QStringLiteral("srcutil"),
                                      QStringLiteral("src/util.cpp")).matched());
    }

    void aWordBoundaryIsNotPreferredIfItStrandsTheRestOfTheQuery()
    {
        // The word-boundary preference jumps forward past earlier occurrences.
        // A jump that leaves the remaining query unsatisfiable must not be
        // taken: here the `e` could go to the `E` of `Editor` - a boundary -
        // but then there is no `w` left, turning a literal prefix into a
        // non-match.
        const FuzzyResult result = FuzzyMatch::match(QStringLiteral("view"),
                                                     QStringLiteral("View Split Editor"));
        QVERIFY(result.matched());
        QCOMPARE(result.positions, (std::vector<int>{0, 1, 2, 3}));
    }

    void aPrefixAlwaysMatches()
    {
        // The general form of the case above: if the query is a literal prefix
        // of the candidate, no alignment choice may reject it.
        const QString candidate = QStringLiteral("View Split Editor");
        for (int length = 1; length <= candidate.size(); ++length) {
            const QString query = candidate.left(length);
            QVERIFY2(FuzzyMatch::match(query, candidate).matched(),
                     qPrintable(query));
        }
    }

    // ---- File index --------------------------------------------------------

    void indexesTheProjectsFiles()
    {
        writeFile(QStringLiteral("a.txt"), QStringLiteral("x"));
        writeFile(QStringLiteral("src/b.cpp"), QStringLiteral("x"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        QCOMPARE(m_index->fileCount(), 2);
    }

    void storesRelativePathsAndFileNames()
    {
        writeFile(QStringLiteral("src/deep/file.cpp"), QStringLiteral("x"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());
        QCOMPARE(m_index->fileCount(), 1);

        const IndexedFile& file = m_index->files().front();
        QCOMPARE(file.relativePath, QStringLiteral("src/deep/file.cpp"));
        QCOMPARE(file.fileName, QStringLiteral("file.cpp"));
    }

    void honoursIgnoreRules()
    {
        // Indexing node_modules would bury the project's own files and cost far
        // more time than the rest of the walk.
        writeFile(QStringLiteral(".gitignore"), QStringLiteral("ignored/\n*.log\n"));
        writeFile(QStringLiteral("kept.cpp"), QStringLiteral("x"));
        writeFile(QStringLiteral("ignored/hidden.cpp"), QStringLiteral("x"));
        writeFile(QStringLiteral("noisy.log"), QStringLiteral("x"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        QStringList names;
        for (const IndexedFile& file : m_index->files()) {
            names.append(file.fileName);
        }

        QVERIFY(names.contains(QStringLiteral("kept.cpp")));
        QVERIFY(!names.contains(QStringLiteral("hidden.cpp")));
        QVERIFY(!names.contains(QStringLiteral("noisy.log")));
    }

    void closingTheProjectEmptiesTheIndex()
    {
        writeFile(QStringLiteral("a.txt"), QStringLiteral("x"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());
        QVERIFY(m_index->fileCount() > 0);

        m_project->close();
        QCOMPARE(m_index->fileCount(), 0);
    }

    void addAndRemoveKeepTheIndexCurrent()
    {
        // The watcher reports changes; the index must not need a full rebuild.
        writeFile(QStringLiteral("a.txt"), QStringLiteral("x"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());
        QCOMPARE(m_index->fileCount(), 1);

        writeFile(QStringLiteral("b.txt"), QStringLiteral("x"));
        m_index->addFile(path(QStringLiteral("b.txt")));
        QCOMPARE(m_index->fileCount(), 2);

        // Reporting the same creation twice must not duplicate the entry.
        m_index->addFile(path(QStringLiteral("b.txt")));
        QCOMPARE(m_index->fileCount(), 2);

        m_index->removeFile(path(QStringLiteral("b.txt")));
        QCOMPARE(m_index->fileCount(), 1);
    }

    void aFileOutsideTheProjectIsNotIndexed()
    {
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        m_index->addFile(QStringLiteral("/somewhere/else/file.cpp"));
        QCOMPARE(m_index->fileCount(), 0);
    }

    // ---- Text search -------------------------------------------------------

    void findsMatchesAcrossFiles()
    {
        writeFile(QStringLiteral("a.cpp"), QStringLiteral("int needle = 1;\nint other = 2;"));
        writeFile(QStringLiteral("b.cpp"), QStringLiteral("// needle again"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("needle");
        search.search(options);

        QVERIFY(waitFor([&search] { return !search.isSearching(); }));
        QCOMPARE(search.resultCount(), 2);
    }

    void reportsLineAndColumn()
    {
        writeFile(QStringLiteral("a.cpp"),
                  QStringLiteral("first line\nsecond has target here"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("target");
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));

        QCOMPARE(search.resultCount(), 1);
        const SearchMatch& match = search.results().front();
        QCOMPARE(match.line, 1);
        QCOMPARE(match.column, 11);
        QCOMPARE(match.length, 6);
        QCOMPARE(match.lineText, QStringLiteral("second has target here"));
    }

    void findsEveryMatchOnALine()
    {
        // Each is a distinct place to jump to.
        writeFile(QStringLiteral("a.cpp"), QStringLiteral("x x x"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("x");
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));

        QCOMPARE(search.resultCount(), 3);
    }

    void isCaseInsensitiveByDefault()
    {
        writeFile(QStringLiteral("a.cpp"), QStringLiteral("NEEDLE"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("needle");
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));
        QCOMPARE(search.resultCount(), 1);

        options.caseSensitive = true;
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));
        QCOMPARE(search.resultCount(), 0);
    }

    void treatsAPlainQueryLiterally()
    {
        // A query containing regex syntax must search for those characters, not
        // be reinterpreted - otherwise searching for "a.b" matches "axb".
        writeFile(QStringLiteral("a.cpp"), QStringLiteral("value a.b here\nand axb too"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("a.b");
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));

        QCOMPARE(search.resultCount(), 1);
        QCOMPARE(search.results().front().line, 0);
    }

    void supportsRegularExpressions()
    {
        writeFile(QStringLiteral("a.cpp"), QStringLiteral("value 42 here\nno digits"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("[0-9]+");
        options.regularExpression = true;
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));

        QCOMPARE(search.resultCount(), 1);
    }

    void anInvalidRegularExpressionFindsNothingQuietly()
    {
        // This is what the user types while still typing it.
        writeFile(QStringLiteral("a.cpp"), QStringLiteral("text"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("[unclosed");
        options.regularExpression = true;

        QSignalSpy spy(&search, &TextSearch::finished);
        search.search(options);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(search.resultCount(), 0);
    }

    void wholeWordMatchesOnlyCompleteWords()
    {
        writeFile(QStringLiteral("a.cpp"), QStringLiteral("cat\nconcatenate"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("cat");
        options.wholeWord = true;
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));

        QCOMPARE(search.resultCount(), 1);
        QCOMPARE(search.results().front().line, 0);
    }

    void includeAndExcludeGlobsFilterFiles()
    {
        writeFile(QStringLiteral("keep.cpp"), QStringLiteral("target"));
        writeFile(QStringLiteral("skip.txt"), QStringLiteral("target"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);

        SearchOptions options;
        options.query = QStringLiteral("target");
        options.includeGlobs = {QStringLiteral("*.cpp")};
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));
        QCOMPARE(search.resultCount(), 1);

        // Excludes win over includes.
        options.includeGlobs.clear();
        options.excludeGlobs = {QStringLiteral("*.txt")};
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));
        QCOMPARE(search.resultCount(), 1);
    }

    void skipsBinaryFiles()
    {
        // Searching them wastes time and fills results with unreadable lines.
        writeFile(QStringLiteral("text.cpp"), QStringLiteral("target"));

        QFile binary(path(QStringLiteral("data.bin")));
        QVERIFY(binary.open(QIODevice::WriteOnly));
        binary.write(QByteArray("target\0\0\0binary", 15));
        binary.close();

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("target");
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));

        QCOMPARE(search.resultCount(), 1);
        QCOMPARE(search.results().front().relativePath, QStringLiteral("text.cpp"));
    }

    void stopsAtTheResultLimitAndSaysSo()
    {
        // A broad query would otherwise collect hundreds of thousands of lines
        // that nobody reads.
        QString body;
        for (int i = 0; i < 200; ++i) {
            body += QStringLiteral("match here\n");
        }
        writeFile(QStringLiteral("many.txt"), body);

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("match");
        options.maxResults = 50;
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));

        QCOMPARE(search.resultCount(), 50);
        QVERIFY2(search.wasTruncated(),
                 "the UI must be able to say the list is incomplete");
    }

    void cancellingClearsResults()
    {
        writeFile(QStringLiteral("a.cpp"), QStringLiteral("target"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        SearchOptions options;
        options.query = QStringLiteral("target");
        search.search(options);
        QVERIFY(waitFor([&search] { return !search.isSearching(); }));
        QVERIFY(search.resultCount() > 0);

        search.cancel();
        QCOMPARE(search.resultCount(), 0);
        QVERIFY(!search.isSearching());
    }

    void anEmptyQueryFindsNothing()
    {
        writeFile(QStringLiteral("a.cpp"), QStringLiteral("text"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        TextSearch search(*m_project, *m_index, *m_scheduler);
        search.search(SearchOptions{});
        QCOMPARE(search.resultCount(), 0);
    }

    // ---- Performance budgets ----------------------------------------------
    //
    // The architecture sets these; measuring them here means a regression shows
    // up as a failing test rather than as a slow editor nobody profiles.

    void quickOpenScoringMeetsItsBudget()
    {
        // Budget: first results within 100 ms on 50,000 files (architecture
        // section 8). This measures the scoring pass, which is what runs on
        // every keystroke.
        //
        // The budget applies to the release build, which is what users run.
        // A debug build is roughly an order of magnitude slower - measured at
        // 838 ms against 76 ms for the same work - so asserting the release
        // number in debug would fail permanently and teach the team to ignore
        // this test. Debug still runs the measurement and reports it, so a
        // regression is visible; only the assertion is release-only.
        std::vector<QString> paths;
        paths.reserve(50000);
        for (int i = 0; i < 50000; ++i) {
            paths.push_back(QStringLiteral("src/module%1/component%2/File%3.cpp")
                                .arg(i % 50).arg(i % 200).arg(i));
        }

        QElapsedTimer timer;
        timer.start();

        int matches = 0;
        for (const QString& candidate : paths) {
            if (FuzzyMatch::matchPath(QStringLiteral("compfile"), candidate).matched()) {
                ++matches;
            }
        }

        const qint64 elapsed = timer.elapsed();
        qInfo() << "scored 50,000 paths in" << elapsed << "ms," << matches << "matched";

        // Every candidate matches here, which is the worst case: a real query
        // rejects most of them on the first missing character.
        QCOMPARE(matches, 50000);

#ifdef QT_NO_DEBUG
        QVERIFY2(elapsed < 100,
                 qPrintable(QStringLiteral("scoring took %1 ms, budget is 100 ms")
                                .arg(elapsed)));
#else
        // Ten times the release budget: loose enough not to fail on an
        // unoptimised build, tight enough to catch an algorithmic regression.
        QVERIFY2(elapsed < 1000,
                 qPrintable(QStringLiteral("scoring took %1 ms in a debug build, "
                                           "which suggests an algorithmic regression")
                                .arg(elapsed)));
#endif
    }

    void indexingALargeTreeIsReasonable()
    {
        // Not a hard budget - it depends on the filesystem - but a regression
        // that makes indexing quadratic would show up here.
        for (int i = 0; i < 500; ++i) {
            writeFile(QStringLiteral("dir%1/file%2.cpp").arg(i % 20).arg(i),
                      QStringLiteral("contents"));
        }

        QElapsedTimer timer;
        timer.start();

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIndex());

        qInfo() << "indexed" << m_index->fileCount() << "files in"
                << timer.elapsed() << "ms";
        QCOMPARE(m_index->fileCount(), 500);
    }
};

QTEST_MAIN(SearchTests)
#include "SearchTests.moc"
