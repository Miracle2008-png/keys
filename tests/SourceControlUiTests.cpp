#include "core/TaskScheduler.h"
#include "filesystem/FileSystem.h"
#include "process/CommandRunner.h"
#include "ui/SourceControlModel.h"
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
#include <type_traits>

using keys::core::TaskScheduler;
using keys::fs::FileSystem;
using keys::process::CommandOptions;
using keys::process::CommandRunner;
using keys::ui::SourceControlModel;
using keys::vcs::GitClient;
using keys::vcs::Repository;

/// The source control panel's model, against a real repository.
///
/// What this pins down is the mapping from a repository's status to the rows the
/// panel draws — in particular the rule that a file staged and then modified
/// again appears in both sections, which is the case a simpler model would get
/// wrong.
class SourceControlUiTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<TaskScheduler> m_scheduler;
    std::unique_ptr<Repository> m_repository;
    std::unique_ptr<SourceControlModel> m_model;

    [[nodiscard]] QString path(const QString& relative) const
    {
        return QDir(m_dir->path()).filePath(relative);
    }

    void writeFile(const QString& relative, const QString& body) const
    {
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(path(relative), body)));
    }

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

    void initRepository() const
    {
        git({QStringLiteral("init"), QStringLiteral("--initial-branch=main")});
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

    [[nodiscard]] QVariant roleAt(int row, const char* name) const
    {
        const QHash<int, QByteArray> names = m_model->roleNames();
        for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
            if (it.value() == name) {
                return m_model->data(m_model->index(row, 0), it.key());
            }
        }
        return {};
    }

    /// The row for `path` in the given section, or -1.
    [[nodiscard]] int rowOf(const QString& file, bool staged) const
    {
        for (int row = 0; row < m_model->count(); ++row) {
            if (roleAt(row, "path").toString() == file
                && roleAt(row, "isStaged").toBool() == staged) {
                return row;
            }
        }
        return -1;
    }

    /// Opens the repository and waits for the first status to arrive.
    void openAndSettle() const
    {
        m_repository->openFor(m_dir->path());
        QVERIFY(m_repository->isOpen());
        QVERIFY(waitFor([this] { return !m_repository->status().branch.isEmpty(); }));
    }

private slots:
    void initTestCase()
    {
        if (!GitClient::isGitAvailable()) {
            qWarning("SourceControlUiTests: git was not found on PATH - EVERY case "
                     "below is skipped and this suite proves nothing.");
            QSKIP("git is not on PATH");
        }
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());

        // See VcsTests: a repository above the temporary directory would make
        // these cases test the wrong tree.
        qputenv("GIT_CEILING_DIRECTORIES",
                QFileInfo(m_dir->path()).absolutePath().toUtf8());

        m_scheduler = std::make_unique<TaskScheduler>();
        m_repository = std::make_unique<Repository>(*m_scheduler);
        m_model = std::make_unique<SourceControlModel>(*m_repository);
    }

    void cleanup()
    {
        qunsetenv("GIT_CEILING_DIRECTORIES");
        m_model.reset();
        m_repository.reset();
        m_scheduler.reset();
        m_dir.reset();
    }

    // ---- Empty states ------------------------------------------------------

    void reportsNoRepositoryForAPlainDirectory()
    {
        m_repository->openFor(m_dir->path());
        QVERIFY(!m_model->hasRepository());
        QCOMPARE(m_model->count(), 0);
        QVERIFY(!m_model->canCommit());
    }

    void reportsTheBranch()
    {
        initRepository();
        openAndSettle();

        QVERIFY(m_model->hasRepository());
        QCOMPARE(m_model->branch(), QStringLiteral("main"));
        QCOMPARE(m_model->count(), 0);   // clean
    }

    // ---- Rows --------------------------------------------------------------

    void listsUnstagedChanges()
    {
        initRepository();
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Changed\n"));
        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() > 0; }));

        const int row = rowOf(QStringLiteral("README.md"), false);
        QVERIFY(row >= 0);
        QCOMPARE(roleAt(row, "fileName").toString(), QStringLiteral("README.md"));
        QCOMPARE(roleAt(row, "statusLetter").toString(), QStringLiteral("M"));
        QVERIFY(!roleAt(row, "isStaged").toBool());
    }

    void splitsTheDirectoryFromTheFileName()
    {
        // The panel shows the name prominently and the directory beside it, so
        // two files with the same name stay distinguishable.
        initRepository();
        QVERIFY(QDir(m_dir->path()).mkpath(QStringLiteral("src/deep")));
        writeFile(QStringLiteral("src/deep/thing.cpp"), QStringLiteral("x\n"));
        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() > 0; }));

        const int row = rowOf(QStringLiteral("src/deep/thing.cpp"), false);
        QVERIFY(row >= 0);
        QCOMPARE(roleAt(row, "fileName").toString(), QStringLiteral("thing.cpp"));
        QCOMPARE(roleAt(row, "directory").toString(), QStringLiteral("src/deep"));
    }

    void aFileStagedAndModifiedAgainAppearsInBothSections()
    {
        // The case a two-model design gets wrong. Both entries are real: one
        // change is going into the commit and the other is not, and hiding
        // either would leave the user unable to act on it.
        initRepository();
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Staged\n"));
        git({QStringLiteral("add"), QStringLiteral("README.md")});
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Staged then changed\n"));

        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() >= 2; }));

        QVERIFY(rowOf(QStringLiteral("README.md"), true) >= 0);
        QVERIFY(rowOf(QStringLiteral("README.md"), false) >= 0);
    }

    void stagedRowsComeFirst()
    {
        initRepository();
        writeFile(QStringLiteral("staged.txt"), QStringLiteral("s\n"));
        git({QStringLiteral("add"), QStringLiteral("staged.txt")});
        writeFile(QStringLiteral("loose.txt"), QStringLiteral("l\n"));

        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() >= 2; }));

        QVERIFY(rowOf(QStringLiteral("staged.txt"), true)
                < rowOf(QStringLiteral("loose.txt"), false));
    }

    void marksTheFirstRowOfEachSection()
    {
        initRepository();
        writeFile(QStringLiteral("staged.txt"), QStringLiteral("s\n"));
        git({QStringLiteral("add"), QStringLiteral("staged.txt")});
        writeFile(QStringLiteral("loose.txt"), QStringLiteral("l\n"));

        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() >= 2; }));

        int starts = 0;
        for (int row = 0; row < m_model->count(); ++row) {
            if (roleAt(row, "isSectionStart").toBool()) {
                ++starts;
            }
        }
        QCOMPARE(starts, 2);   // one heading per section, no more
    }

    // ---- Commit gating -----------------------------------------------------

    void cannotCommitWithNothingStaged()
    {
        initRepository();
        writeFile(QStringLiteral("loose.txt"), QStringLiteral("l\n"));
        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() > 0; }));

        QVERIFY(!m_model->canCommit());
    }

    void canCommitWithSomethingStaged()
    {
        initRepository();
        writeFile(QStringLiteral("staged.txt"), QStringLiteral("s\n"));
        git({QStringLiteral("add"), QStringLiteral("staged.txt")});

        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->stagedCount() > 0; }));

        QVERIFY(m_model->canCommit());
    }

    void cannotCommitDuringAConflict()
    {
        // A merge has to be finished before an ordinary commit means anything,
        // so the button is refused rather than the commit failing later.
        initRepository();

        git({QStringLiteral("checkout"), QStringLiteral("-b"), QStringLiteral("other")});
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Other\n"));
        git({QStringLiteral("commit"), QStringLiteral("-am"), QStringLiteral("Other")});

        git({QStringLiteral("checkout"), QStringLiteral("main")});
        writeFile(QStringLiteral("README.md"), QStringLiteral("# Main\n"));
        git({QStringLiteral("commit"), QStringLiteral("-am"), QStringLiteral("Main")});

        CommandOptions options;
        options.workingDirectory = m_dir->path();
        (void)CommandRunner::run(QStringLiteral("git"),
                                 {QStringLiteral("merge"), QStringLiteral("other")},
                                 options);

        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() > 0; }));

        QCOMPARE(m_model->operationName(), QStringLiteral("merge"));
        QVERIFY(!m_model->canCommit());
    }

    // ---- Actions -----------------------------------------------------------

    void stagingThroughTheModelMovesTheRow()
    {
        initRepository();
        writeFile(QStringLiteral("new.txt"), QStringLiteral("n\n"));
        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() > 0; }));

        QVERIFY(rowOf(QStringLiteral("new.txt"), false) >= 0);

        m_model->stage(QStringLiteral("new.txt"));
        QVERIFY(waitFor([this] { return rowOf(QStringLiteral("new.txt"), true) >= 0; }));
        QCOMPARE(rowOf(QStringLiteral("new.txt"), false), -1);
    }

    void stageAllStagesEverything()
    {
        initRepository();
        writeFile(QStringLiteral("a.txt"), QStringLiteral("a\n"));
        writeFile(QStringLiteral("b.txt"), QStringLiteral("b\n"));
        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() >= 2; }));

        m_model->stageAll();
        QVERIFY(waitFor([this] { return m_model->stagedCount() == 2; }));
        QCOMPARE(m_model->unstagedCount(), 0);
    }

    void activateReportsAnAbsolutePath()
    {
        // Git speaks in paths relative to the repository root; the workbench
        // opens absolute ones, and the two roots are not always the same.
        initRepository();
        writeFile(QStringLiteral("new.txt"), QStringLiteral("n\n"));
        openAndSettle();

        QSignalSpy spy(m_model.get(), &SourceControlModel::fileActivated);
        m_model->activate(QStringLiteral("new.txt"));

        QCOMPARE(spy.count(), 1);
        const QString reported = spy.at(0).at(0).toString();
        QVERIFY(QDir::isAbsolutePath(reported));
        QCOMPARE(QFileInfo(reported).canonicalFilePath(),
                 QFileInfo(path(QStringLiteral("new.txt"))).canonicalFilePath());
    }

    void closingTheProjectEmptiesThePanel()
    {
        initRepository();
        writeFile(QStringLiteral("new.txt"), QStringLiteral("n\n"));
        openAndSettle();
        QVERIFY(waitFor([this] { return m_model->count() > 0; }));

        m_repository->close();
        QVERIFY(!m_model->hasRepository());
        QCOMPARE(m_model->count(), 0);
        QVERIFY(m_model->branch().isEmpty());
    }

    // ---- QML registration --------------------------------------------------

    void theSingletonIsNotEngineConstructible()
    {
        // See ThemeTests::qmlResolvesToThePublishedInstance for what this
        // prevents.
        static_assert(!std::is_default_constructible_v<SourceControlModel>);

        SourceControlModel::setInstance(m_model.get());
        QCOMPARE(SourceControlModel::create(nullptr, nullptr), m_model.get());
        SourceControlModel::setInstance(nullptr);
    }
};

QTEST_MAIN(SourceControlUiTests)
#include "SourceControlUiTests.moc"
