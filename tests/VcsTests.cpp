#include "core/TaskScheduler.h"
#include "filesystem/FileSystem.h"
#include "process/CommandRunner.h"
#include "vcs/GitClient.h"
#include "vcs/Repository.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace keys::vcs;
using keys::core::TaskScheduler;
using keys::fs::FileSystem;
using keys::process::CommandOptions;
using keys::process::CommandRunner;

/// Git integration, against a real repository.
///
/// **Not mocked.** The whole point of driving the git binary (ARCHITECTURE 5.3)
/// is that it behaves exactly as the user's own git does. A mock would test a
/// model of git's output rather than git's output, which is precisely the thing
/// most likely to be wrong. So these build a throwaway repository and run
/// against it, and skip when git is not installed rather than failing.
class VcsTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    [[nodiscard]] QString path(const QString& relative) const
    {
        return QDir(m_dir->path()).filePath(relative);
    }

    void writeFile(const QString& relative, const QString& body) const
    {
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(path(relative), body)));
    }

    /// Runs git in the temporary repository, failing the test if it fails —
    /// these are the calls that set up a case, so a failure is a broken test
    /// rather than a finding.
    void git(const QStringList& arguments) const
    {
        CommandOptions options;
        options.workingDirectory = m_dir->path();
        options.timeoutMs = 30000;

        const auto result = CommandRunner::run(QStringLiteral("git"), arguments, options);
        QVERIFY2(static_cast<bool>(result), "git could not be started");
        QVERIFY2(result.value().succeeded(),
                 qPrintable(QStringLiteral("git %1 failed: %2")
                                .arg(arguments.join(QLatin1Char(' ')),
                                     result.value().errorText())));
    }

    /// A repository with one commit, which is the state most cases start from.
    void initRepository() const
    {
        git({QStringLiteral("init"), QStringLiteral("--initial-branch=main")});

        // Identity and signing are set locally so the test does not depend on
        // the machine's global config, and cannot be derailed by a global
        // commit.gpgsign that would prompt.
        git({QStringLiteral("config"), QStringLiteral("user.email"),
             QStringLiteral("test@keys.dev")});
        git({QStringLiteral("config"), QStringLiteral("user.name"),
             QStringLiteral("Keys Test")});
        git({QStringLiteral("config"), QStringLiteral("commit.gpgsign"),
             QStringLiteral("false")});

        writeFile(QStringLiteral("README.md"), QStringLiteral("# Test\n"));
        git({QStringLiteral("add"), QStringLiteral("README.md")});
        git({QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("Initial")});
    }

    [[nodiscard]] GitClient client() const { return GitClient(m_dir->path()); }

    /// Finds the change for a path, or fails.
    [[nodiscard]] static const FileChange* changeFor(const RepositoryStatus& status,
                                                     const QString& path)
    {
        for (const FileChange& change : status.changes) {
            if (change.path == path) {
                return &change;
            }
        }
        return nullptr;
    }

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

private slots:
    void initTestCase()
    {
        // Skipping is right when git genuinely is not installed - but a skip
        // reports as a pass, so it must be impossible to miss. This is the
        // failure mode that hid the whole suite once already: ctest handed the
        // test a PATH without git, every case skipped, and the summary said the
        // suite passed.
        if (!GitClient::isGitAvailable()) {
            qWarning("VcsTests: git was not found on PATH - EVERY case below is "
                     "skipped and this suite proves nothing.");
            QSKIP("git is not on PATH; the vcs module cannot be tested without it");
        }
        qInfo("VcsTests: using %s", qPrintable(GitClient::gitVersion()));
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());

        // Git walks upward looking for a repository, so a repository anywhere
        // above the temporary directory - including one accidentally created at
        // a drive root, which is exactly the state this machine is in - would
        // make "not a repository" impossible to test. The ceiling stops the walk
        // at the temporary directory, so each case sees only what it created.
        // The ceiling is the parent: git stops walking *above* a ceiling entry,
        // so naming the temporary directory itself would stop the walk before it
        // ever reached it and break lookups from a subdirectory.
        qputenv("GIT_CEILING_DIRECTORIES",
                QFileInfo(m_dir->path()).absolutePath().toUtf8());
    }

    void cleanup()
    {
        qunsetenv("GIT_CEILING_DIRECTORIES");
        m_dir.reset();
    }

    // ---- CommandRunner -----------------------------------------------------

    void aNonZeroExitIsARunNotAFailure()
    {
        // The distinction matters: a command that ran and said no is not the
        // same as one that could not be started, and callers act differently.
        const auto result = CommandRunner::run(
            QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")},
            [this] {
                CommandOptions options;
                options.workingDirectory = m_dir->path();
                return options;
            }());

        QVERIFY(static_cast<bool>(result));      // it ran
        QVERIFY(!result.value().succeeded());    // and reported failure
        QVERIFY(!result.value().errorText().isEmpty());
    }

    void aMissingProgramIsAFailure()
    {
        CommandOptions options;
        options.timeoutMs = 5000;
        const auto result = CommandRunner::run(
            QStringLiteral("keys-no-such-program-exists"), {}, options);
        QVERIFY(!result);
    }

    // ---- Discovery ---------------------------------------------------------

    void findsTheRepositoryRoot()
    {
        initRepository();
        QVERIFY(QDir(m_dir->path()).mkpath(QStringLiteral("src/deep")));

        // From a subdirectory: git walks up, which is why this asks git rather
        // than looking for a .git directory itself.
        const auto found = GitClient::findRepositoryRoot(path(QStringLiteral("src/deep")));
        QVERIFY(static_cast<bool>(found));

        // Compared through QFileInfo so a symlinked temp directory (macOS
        // /var -> /private/var, and Windows short paths) does not fail this.
        QCOMPARE(QFileInfo(found.value()).canonicalFilePath(),
                 QFileInfo(m_dir->path()).canonicalFilePath());
    }

    void reportsNotFoundOutsideARepository()
    {
        // A project without git is ordinary, so this must be a plain NotFound
        // rather than something the UI shows as an error.
        const auto found = GitClient::findRepositoryRoot(m_dir->path());
        QVERIFY(!found);
        QCOMPARE(found.error().code(), keys::core::ErrorCode::NotFound);
    }

    // ---- Status parsing ----------------------------------------------------

    void readsBranchAndCleanState()
    {
        initRepository();

        const auto status = client().status();
        QVERIFY(static_cast<bool>(status));
        QCOMPARE(status.value().branch, QStringLiteral("main"));
        QVERIFY(status.value().isClean());
        QVERIFY(!status.value().isDetached());
        QVERIFY(!status.value().hasUpstream());
    }

    void reportsUntrackedFiles()
    {
        initRepository();
        writeFile(QStringLiteral("new.txt"), QStringLiteral("hello\n"));

        const auto status = client().status();
        QVERIFY(static_cast<bool>(status));

        const FileChange* change = changeFor(status.value(), QStringLiteral("new.txt"));
        QVERIFY(change != nullptr);
        QCOMPARE(change->unstaged, FileStatus::Untracked);
        QVERIFY(!change->hasStagedChange());
    }

    void reportsUntrackedFilesInsideNewDirectories()
    {
        // --untracked-files=all, so a new folder does not collapse into one
        // entry the user cannot stage a single file from.
        initRepository();
        QVERIFY(QDir(m_dir->path()).mkpath(QStringLiteral("fresh")));
        writeFile(QStringLiteral("fresh/a.txt"), QStringLiteral("a\n"));
        writeFile(QStringLiteral("fresh/b.txt"), QStringLiteral("b\n"));

        const auto status = client().status();
        QVERIFY(static_cast<bool>(status));
        QVERIFY(changeFor(status.value(), QStringLiteral("fresh/a.txt")) != nullptr);
        QVERIFY(changeFor(status.value(), QStringLiteral("fresh/b.txt")) != nullptr);
    }

    void separatesStagedFromUnstagedChanges()
    {
        // The two-letter code is why both sides are modelled: this file is
        // staged as modified and then modified again, and collapsing that would
        // have to misreport one of them.
        initRepository();
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Staged\n"));
        git({QStringLiteral("add"), QStringLiteral("README.md")});
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Staged then changed\n"));

        const auto status = client().status();
        QVERIFY(static_cast<bool>(status));

        const FileChange* change = changeFor(status.value(), QStringLiteral("README.md"));
        QVERIFY(change != nullptr);
        QCOMPARE(change->staged, FileStatus::Modified);
        QCOMPARE(change->unstaged, FileStatus::Modified);
        QVERIFY(change->hasStagedChange());
        QVERIFY(change->hasUnstagedChange());
    }

    void readsPathsContainingSpaces()
    {
        // The porcelain path is the remainder of the record, not a single
        // space-delimited field - a distinction only a path with a space shows.
        initRepository();
        writeFile(QStringLiteral("a file with spaces.txt"), QStringLiteral("x\n"));

        const auto status = client().status();
        QVERIFY(static_cast<bool>(status));
        QVERIFY(changeFor(status.value(),
                          QStringLiteral("a file with spaces.txt")) != nullptr);
    }

    void readsRenamesWithTheirOriginalPath()
    {
        initRepository();
        git({QStringLiteral("mv"), QStringLiteral("README.md"),
             QStringLiteral("DOCS.md")});

        const auto status = client().status();
        QVERIFY(static_cast<bool>(status));

        const FileChange* change = changeFor(status.value(), QStringLiteral("DOCS.md"));
        QVERIFY(change != nullptr);
        QCOMPARE(change->staged, FileStatus::Renamed);
        // The original path is the next NUL record, not part of the first.
        QCOMPARE(change->originalPath, QStringLiteral("README.md"));
    }

    void countsStagedAndUnstagedSeparately()
    {
        initRepository();
        writeFile(QStringLiteral("staged.txt"), QStringLiteral("s\n"));
        git({QStringLiteral("add"), QStringLiteral("staged.txt")});
        writeFile(QStringLiteral("loose.txt"), QStringLiteral("l\n"));

        const auto status = client().status();
        QVERIFY(static_cast<bool>(status));
        QCOMPARE(status.value().stagedCount(), 1);
        QCOMPARE(status.value().unstagedCount(), 1);
    }

    void detectsADetachedHead()
    {
        initRepository();
        const auto client = this->client();

        const auto log = client.log(1);
        QVERIFY(static_cast<bool>(log));
        QCOMPARE(log.value().size(), size_t{1});

        git({QStringLiteral("checkout"), log.value().front().hash});

        const auto status = client.status();
        QVERIFY(static_cast<bool>(status));
        QVERIFY(status.value().isDetached());
        // There is no branch name to show, so the commit is what the UI shows.
        QVERIFY(!status.value().headCommit.isEmpty());
    }

    void detectsAnInProgressMerge()
    {
        initRepository();

        // Two branches changing the same line, so merging conflicts.
        git({QStringLiteral("checkout"), QStringLiteral("-b"), QStringLiteral("other")});
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Other\n"));
        git({QStringLiteral("commit"), QStringLiteral("-am"), QStringLiteral("Other")});

        git({QStringLiteral("checkout"), QStringLiteral("main")});
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Main\n"));
        git({QStringLiteral("commit"), QStringLiteral("-am"), QStringLiteral("Main")});

        // Expected to fail: that is the conflict this case is about.
        CommandOptions options;
        options.workingDirectory = m_dir->path();
        (void)CommandRunner::run(QStringLiteral("git"),
                                 {QStringLiteral("merge"), QStringLiteral("other")},
                                 options);

        const auto status = client().status();
        QVERIFY(static_cast<bool>(status));
        QVERIFY(status.value().operationInProgress);
        QCOMPARE(status.value().operationName, QStringLiteral("merge"));
        QVERIFY(status.value().hasConflicts());
    }

    // ---- Log ---------------------------------------------------------------

    void readsCommitsNewestFirst()
    {
        initRepository();
        writeFile(QStringLiteral("second.txt"), QStringLiteral("2\n"));
        git({QStringLiteral("add"), QStringLiteral(".")});
        git({QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("Second")});

        const auto log = client().log(10);
        QVERIFY(static_cast<bool>(log));
        QCOMPARE(log.value().size(), size_t{2});
        QCOMPARE(log.value().at(0).subject, QStringLiteral("Second"));
        QCOMPARE(log.value().at(1).subject, QStringLiteral("Initial"));
        QCOMPARE(log.value().at(0).author, QStringLiteral("Keys Test"));
        QVERIFY(!log.value().at(0).shortHash.isEmpty());
    }

    void readsSubjectsContainingSeparatorLikeText()
    {
        // The format uses control characters precisely because a subject can
        // contain anything a person can type.
        initRepository();
        writeFile(QStringLiteral("x.txt"), QStringLiteral("x\n"));
        git({QStringLiteral("add"), QStringLiteral(".")});
        git({QStringLiteral("commit"), QStringLiteral("-m"),
             QStringLiteral("Fix: a|b, c\\d and \"quotes\"")});

        const auto log = client().log(1);
        QVERIFY(static_cast<bool>(log));
        QCOMPARE(log.value().at(0).subject,
                 QStringLiteral("Fix: a|b, c\\d and \"quotes\""));
    }

    // ---- Staging and committing --------------------------------------------

    void stagingAndUnstagingRoundTrip()
    {
        initRepository();
        writeFile(QStringLiteral("new.txt"), QStringLiteral("n\n"));

        const GitClient client = this->client();
        QVERIFY(static_cast<bool>(client.stage({QStringLiteral("new.txt")})));

        auto status = client.status();
        QVERIFY(static_cast<bool>(status));
        QCOMPARE(changeFor(status.value(), QStringLiteral("new.txt"))->staged,
                 FileStatus::Added);

        QVERIFY(static_cast<bool>(client.unstage({QStringLiteral("new.txt")})));

        status = client.status();
        QVERIFY(static_cast<bool>(status));
        // Back to untracked, not gone: unstaging must not delete the file.
        QCOMPARE(changeFor(status.value(), QStringLiteral("new.txt"))->unstaged,
                 FileStatus::Untracked);
    }

    void discardRestoresTheWorkingTree()
    {
        initRepository();
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Changed\n"));

        const GitClient client = this->client();
        QVERIFY(static_cast<bool>(client.discard({QStringLiteral("README.md")})));

        const auto status = client.status();
        QVERIFY(static_cast<bool>(status));
        QVERIFY(status.value().isClean());
    }

    void discardRefusesAnEmptySelection()
    {
        // "Discard nothing" must not be read as "discard everything": an empty
        // selection is what an accidental click produces.
        initRepository();
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Changed\n"));

        const GitClient client = this->client();
        QVERIFY(!client.discard({}));

        const auto status = client.status();
        QVERIFY(static_cast<bool>(status));
        QVERIFY(!status.value().isClean());   // still there
    }

    void commitRequiresAMessage()
    {
        initRepository();
        writeFile(QStringLiteral("new.txt"), QStringLiteral("n\n"));

        const GitClient client = this->client();
        QVERIFY(static_cast<bool>(client.stage({QStringLiteral("new.txt")})));
        QVERIFY(!client.commit(QStringLiteral("   ")));
    }

    void commitFailsWithNothingStaged()
    {
        // Rather than creating an empty commit, which is never what a click on
        // a disabled-looking button meant.
        initRepository();
        QVERIFY(!client().commit(QStringLiteral("Nothing here")));
    }

    void commitRecordsWhatWasStaged()
    {
        initRepository();
        writeFile(QStringLiteral("new.txt"), QStringLiteral("n\n"));

        const GitClient client = this->client();
        QVERIFY(static_cast<bool>(client.stage({QStringLiteral("new.txt")})));
        QVERIFY(static_cast<bool>(client.commit(QStringLiteral("Add new file"))));

        const auto status = client.status();
        QVERIFY(static_cast<bool>(status));
        QVERIFY(status.value().isClean());

        const auto log = client.log(1);
        QVERIFY(static_cast<bool>(log));
        QCOMPARE(log.value().at(0).subject, QStringLiteral("Add new file"));
    }

    void diffShowsTheChange()
    {
        initRepository();
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Changed\n"));

        const auto diff = client().diff(QStringLiteral("README.md"), false);
        QVERIFY(static_cast<bool>(diff));
        QVERIFY(diff.value().contains(QStringLiteral("-# Test")));
        QVERIFY(diff.value().contains(QStringLiteral("+# Changed")));
    }

    // ---- Repository --------------------------------------------------------

    void repositoryRefreshesInTheBackground()
    {
        initRepository();

        TaskScheduler scheduler;
        Repository repository(scheduler);

        QSignalSpy statusSpy(&repository, &Repository::statusChanged);
        repository.openFor(m_dir->path());
        QVERIFY(repository.isOpen());

        QVERIFY(waitFor([&statusSpy] { return statusSpy.count() > 0; }));
        QCOMPARE(repository.status().branch, QStringLiteral("main"));
        QVERIFY(!repository.isRefreshing());
    }

    void repositoryIsClosedForAProjectWithoutGit()
    {
        TaskScheduler scheduler;
        Repository repository(scheduler);

        repository.openFor(m_dir->path());
        QVERIFY(!repository.isOpen());
        QVERIFY(repository.root().isEmpty());
    }

    void burstsOfRefreshesCollapse()
    {
        // A build touching many files must not queue one status run per file.
        initRepository();

        TaskScheduler scheduler;
        Repository repository(scheduler);
        repository.openFor(m_dir->path());

        QSignalSpy statusSpy(&repository, &Repository::statusChanged);
        QVERIFY(waitFor([&statusSpy] { return statusSpy.count() > 0; }));

        const int afterFirst = statusSpy.count();
        for (int i = 0; i < 50; ++i) {
            repository.refresh();
        }

        QVERIFY(waitFor([&] { return statusSpy.count() > afterFirst; }));
        // Settle, then confirm fifty requests did not become fifty refreshes.
        QVERIFY(!waitFor([&] { return statusSpy.count() > afterFirst + 2; }, 1500));
    }

    void closingDiscardsAnInFlightRefresh()
    {
        // A slow status on a closed project must not deliver into the next one.
        initRepository();

        TaskScheduler scheduler;
        Repository repository(scheduler);
        repository.openFor(m_dir->path());
        repository.close();

        QVERIFY(!repository.isOpen());
        QVERIFY(repository.status().branch.isEmpty());

        // Give any in-flight work time to land, and confirm it did not.
        QVERIFY(!waitFor([&repository] { return !repository.status().branch.isEmpty(); },
                         1500));
    }

    void actionsReportGitsOwnMessage()
    {
        initRepository();

        TaskScheduler scheduler;
        Repository repository(scheduler);
        repository.openFor(m_dir->path());

        QSignalSpy failureSpy(&repository, &Repository::actionFailed);
        repository.commit(QStringLiteral("Nothing is staged"));

        QVERIFY(waitFor([&failureSpy] { return failureSpy.count() > 0; }));
        QCOMPARE(failureSpy.at(0).at(0).toString(), QStringLiteral("commit"));
        QVERIFY(!failureSpy.at(0).at(1).toString().isEmpty());
    }

    void stagingThroughTheRepositoryUpdatesStatus()
    {
        initRepository();
        writeFile(QStringLiteral("new.txt"), QStringLiteral("n\n"));

        TaskScheduler scheduler;
        Repository repository(scheduler);
        repository.openFor(m_dir->path());

        QVERIFY(waitFor([&repository] { return !repository.status().changes.empty(); }));

        repository.stage({QStringLiteral("new.txt")});
        QVERIFY(waitFor([&repository] { return repository.status().stagedCount() == 1; }));
    }
};

QTEST_MAIN(VcsTests)
#include "VcsTests.moc"
