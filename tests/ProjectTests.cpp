#include "config/Settings.h"
#include "filesystem/FileSystem.h"
#include "project/IgnoreRules.h"
#include "project/Project.h"
#include "workspace/RecentProjects.h"
#include "core/TaskScheduler.h"
#include "workspace/Workspace.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace keys::project;
using namespace keys::workspace;
using keys::config::Settings;
using keys::core::TaskScheduler;
using keys::core::ErrorCode;
using keys::core::Status;
using keys::fs::FileSystem;

class ProjectTests : public QObject {
    Q_OBJECT

private:
    /// A fresh directory per test; see FileSystemTests for why sharing one is a
    /// trap. It matters more here: a marker file written by one case would change
    /// the detected project kind in the next.
    std::unique_ptr<QTemporaryDir> m_dir;

    /// Workspace now owns the explorer's model, which needs a worker pool.
    /// One per test, so a task from an earlier case cannot outlive its fixture.
    std::unique_ptr<TaskScheduler> m_scheduler;

    [[nodiscard]] QString path(const QString& relative) const
    {
        return QDir(m_dir->path()).filePath(relative);
    }

    void writeFile(const QString& relative, const QString& content = QStringLiteral("x")) const
    {
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(path(relative), content)));
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_scheduler = std::make_unique<TaskScheduler>();
    }

    void cleanup()
    {
        m_scheduler.reset();
        m_dir.reset();
    }

    // ---- Ignore rules -----------------------------------------------------

    void builtinRulesSkipHeavyDirectories()
    {
        const IgnoreRules rules;
        QVERIFY(rules.isIgnored(QStringLiteral(".git"), true));
        QVERIFY(rules.isIgnored(QStringLiteral("node_modules"), true));
        QVERIFY(rules.isIgnored(QStringLiteral("node_modules/react/index.js"), false));
        QVERIFY(!rules.isIgnored(QStringLiteral("src/main.cpp"), false));
    }

    void globPatternMatchesAtAnyDepth()
    {
        // An unanchored pattern applies to every directory level - that is what
        // makes "*.log" behave the way developers expect.
        IgnoreRules rules;
        rules.clear();
        rules.addPatterns(QStringLiteral("*.log"));

        QVERIFY(rules.isIgnored(QStringLiteral("debug.log"), false));
        QVERIFY(rules.isIgnored(QStringLiteral("src/deep/trace.log"), false));
        QVERIFY(!rules.isIgnored(QStringLiteral("src/main.cpp"), false));
    }

    void anchoredPatternOnlyMatchesAtTheRoot()
    {
        IgnoreRules rules;
        rules.clear();
        rules.addPatterns(QStringLiteral("/build"));

        QVERIFY(rules.isIgnored(QStringLiteral("build"), true));
        // A nested "build" is a different directory and must survive.
        QVERIFY(!rules.isIgnored(QStringLiteral("src/build"), true));
    }

    void directoryOnlyPatternDoesNotMatchAFileOfTheSameName()
    {
        // "build/" means the directory. A file called "build" is unrelated and
        // must stay visible.
        IgnoreRules rules;
        rules.clear();
        rules.addPatterns(QStringLiteral("build/"));

        QVERIFY(rules.isIgnored(QStringLiteral("build"), true));
        QVERIFY(rules.isIgnored(QStringLiteral("build/output.o"), false));
        QVERIFY(!rules.isIgnored(QStringLiteral("build"), false));
    }

    void negationReincludesAPreviouslyIgnoredPath()
    {
        IgnoreRules rules;
        rules.clear();
        rules.addPatterns(QStringLiteral("*.log\n!important.log"));

        QVERIFY(rules.isIgnored(QStringLiteral("debug.log"), false));
        QVERIFY(!rules.isIgnored(QStringLiteral("important.log"), false));
    }

    void laterRulesOverrideEarlierOnes()
    {
        IgnoreRules rules;
        rules.clear();
        rules.addPatterns(QStringLiteral("!keep.log\n*.log"));

        // The re-include comes first, so the later blanket rule wins.
        QVERIFY(rules.isIgnored(QStringLiteral("keep.log"), false));
    }

    void commentsAndBlankLinesAreSkipped()
    {
        IgnoreRules rules;
        rules.clear();
        rules.addPatterns(QStringLiteral("# a comment\n\n   \n*.tmp"));

        QCOMPARE(rules.patternCount(), 1);
        QVERIFY(rules.isIgnored(QStringLiteral("scratch.tmp"), false));
    }

    void regexMetacharactersInNamesAreLiteral()
    {
        // A filename containing "+" or "(" must not be reinterpreted as syntax.
        IgnoreRules rules;
        rules.clear();
        rules.addPatterns(QStringLiteral("a+b.txt"));

        QVERIFY(rules.isIgnored(QStringLiteral("a+b.txt"), false));
        QVERIFY(!rules.isIgnored(QStringLiteral("aab.txt"), false));
    }

    // ---- Project ----------------------------------------------------------

    void openingReportsNameAndRoot()
    {
        Project project;
        QSignalSpy spy(&project, &Project::opened);

        QVERIFY(static_cast<bool>(project.open(m_dir->path())));
        QVERIFY(project.isOpen());
        QCOMPARE(project.root(), FileSystem::normalize(m_dir->path()));
        QCOMPARE(project.name(), QFileInfo(m_dir->path()).fileName());
        QCOMPARE(spy.count(), 1);
    }

    void openingAMissingFolderFails()
    {
        Project project;
        const Status status = project.open(path(QStringLiteral("absent")));
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::NotFound);
        QVERIFY(!project.isOpen());
    }

    void openingAFileRatherThanAFolderFails()
    {
        writeFile(QStringLiteral("a-file.txt"));

        Project project;
        const Status status = project.open(path(QStringLiteral("a-file.txt")));
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::InvalidArgument);
    }

    void projectKindIsInferredFromMarkerFiles()
    {
        writeFile(QStringLiteral("CMakeLists.txt"));

        Project project;
        QVERIFY(static_cast<bool>(project.open(m_dir->path())));
        QCOMPARE(project.kind(), ProjectKind::Cpp);
    }

    void moreSpecificMarkerWins()
    {
        // A Rust project with a package.json for web assets is still Rust.
        writeFile(QStringLiteral("Cargo.toml"));
        writeFile(QStringLiteral("package.json"));

        Project project;
        QVERIFY(static_cast<bool>(project.open(m_dir->path())));
        QCOMPARE(project.kind(), ProjectKind::Rust);
    }

    void projectWithNoMarkerIsStillValid()
    {
        Project project;
        QVERIFY(static_cast<bool>(project.open(m_dir->path())));
        QCOMPARE(project.kind(), ProjectKind::Unknown);
        QVERIFY(project.isOpen());
    }

    void rootGitignoreIsLoaded()
    {
        writeFile(QStringLiteral(".gitignore"), QStringLiteral("*.secret\n"));

        Project project;
        QVERIFY(static_cast<bool>(project.open(m_dir->path())));
        QVERIFY(project.ignoreRules().isIgnored(QStringLiteral("keys.secret"), false));
    }

    void containsRejectsSiblingWithSharedPrefix()
    {
        // "/home/proj-backup" must not count as inside "/home/proj".
        Project project;
        QVERIFY(static_cast<bool>(project.open(m_dir->path())));

        const QString root = project.root();
        QVERIFY(project.contains(root + QStringLiteral("/src/main.cpp")));
        QVERIFY(project.contains(root));
        QVERIFY(!project.contains(root + QStringLiteral("-backup/file.txt")));
        QVERIFY(!project.contains(QStringLiteral("/somewhere/else")));
    }

    void relativePathIsEmptyForOutsidePaths()
    {
        Project project;
        QVERIFY(static_cast<bool>(project.open(m_dir->path())));

        QCOMPARE(project.relativePath(project.root() + QStringLiteral("/src/a.cpp")),
                 QStringLiteral("src/a.cpp"));
        QVERIFY(project.relativePath(QStringLiteral("/elsewhere/a.cpp")).isEmpty());
    }

    void closingResetsEverything()
    {
        writeFile(QStringLiteral("Cargo.toml"));

        Project project;
        QVERIFY(static_cast<bool>(project.open(m_dir->path())));

        QSignalSpy spy(&project, &Project::closed);
        project.close();

        QCOMPARE(spy.count(), 1);
        QVERIFY(!project.isOpen());
        QVERIFY(project.root().isEmpty());
        QCOMPARE(project.kind(), ProjectKind::Unknown);
    }

    // ---- Recent projects --------------------------------------------------

    void recordMovesAnExistingEntryToTheFront()
    {
        QVERIFY(static_cast<bool>(
            FileSystem::createDirectory(path(QStringLiteral("one")))));
        QVERIFY(static_cast<bool>(
            FileSystem::createDirectory(path(QStringLiteral("two")))));

        RecentProjects recent;
        recent.record(path(QStringLiteral("one")));
        recent.record(path(QStringLiteral("two")));
        recent.record(path(QStringLiteral("one")));

        QCOMPARE(recent.count(), 2);
        QCOMPARE(recent.entries().at(0).name, QStringLiteral("one"));
    }

    void recentListIsTrimmedToItsLimit()
    {
        RecentProjects recent;
        for (int i = 0; i < RecentProjects::kMaxEntries + 5; ++i) {
            recent.record(path(QStringLiteral("project-%1").arg(i)));
        }
        QCOMPARE(recent.count(), RecentProjects::kMaxEntries);
    }

    void loadingDropsProjectsThatNoLongerExist()
    {
        // Offering a project that cannot open would waste the user's click.
        const QString present = path(QStringLiteral("still-here"));
        QVERIFY(static_cast<bool>(FileSystem::createDirectory(present)));

        RecentProjects saved;
        saved.record(present);
        saved.record(path(QStringLiteral("deleted-since")));
        QCOMPARE(saved.count(), 2);

        const QString file = path(QStringLiteral("recent.json"));
        QVERIFY(static_cast<bool>(saved.save(file)));

        RecentProjects loaded;
        QVERIFY(static_cast<bool>(loaded.load(file)));
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(loaded.entries().at(0).path, FileSystem::normalize(present));
    }

    void missingHistoryFileIsNotAnError()
    {
        RecentProjects recent;
        QVERIFY(static_cast<bool>(recent.load(path(QStringLiteral("absent.json")))));
        QCOMPARE(recent.count(), 0);
    }

    // ---- Workspace --------------------------------------------------------

    void openingAProjectStartsWatchingItsRoot()
    {
        Settings settings;
        Workspace workspace(settings, *m_scheduler);

        QSignalSpy spy(&workspace, &Workspace::projectOpened);
        QVERIFY(static_cast<bool>(workspace.openProject(m_dir->path())));

        QCOMPARE(spy.count(), 1);
        QVERIFY(workspace.hasProject());
        QCOMPARE(workspace.watcher().watchedDirectoryCount(), 1);
        QCOMPARE(workspace.recentProjects().count(), 1);
    }

    void closingReleasesWatchesAndWorkspaceSettings()
    {
        // A closed project must stop reporting changes, and its settings must not
        // leak into whatever is opened next.
        Settings settings;
        Workspace workspace(settings, *m_scheduler);
        QVERIFY(static_cast<bool>(workspace.openProject(m_dir->path())));

        QVERIFY(static_cast<bool>(settings.setValue(QStringLiteral("editor.tabSize"), 2,
                                                    Settings::Layer::Workspace)));
        QCOMPARE(settings.intValue(QStringLiteral("editor.tabSize")), 2);

        QSignalSpy spy(&workspace, &Workspace::projectClosed);
        workspace.closeProject();

        QCOMPARE(spy.count(), 1);
        QVERIFY(!workspace.hasProject());
        QCOMPARE(workspace.watcher().watchedDirectoryCount(), 0);
        QCOMPARE(settings.intValue(QStringLiteral("editor.tabSize")), 4);
    }

    void openingASecondProjectReplacesTheFirst()
    {
        const QString second = path(QStringLiteral("second-project"));
        QVERIFY(static_cast<bool>(FileSystem::createDirectory(second)));

        Settings settings;
        Workspace workspace(settings, *m_scheduler);

        QVERIFY(static_cast<bool>(workspace.openProject(m_dir->path())));
        QVERIFY(static_cast<bool>(workspace.openProject(second)));

        QCOMPARE(workspace.project().root(), FileSystem::normalize(second));
        // Exactly one root is watched; the previous project's watch is released.
        QCOMPARE(workspace.watcher().watchedDirectoryCount(), 1);
    }

    void failedOpenLeavesNothingOpen()
    {
        Settings settings;
        Workspace workspace(settings, *m_scheduler);

        const Status status = workspace.openProject(path(QStringLiteral("absent")));
        QVERIFY(!static_cast<bool>(status));
        QVERIFY(!workspace.hasProject());
        QCOMPARE(workspace.watcher().watchedDirectoryCount(), 0);
    }

    void workspaceSettingsRoundTripThroughTheProjectFolder()
    {
        Settings settings;
        {
            Workspace workspace(settings, *m_scheduler);
            QVERIFY(static_cast<bool>(workspace.openProject(m_dir->path())));
            QVERIFY(static_cast<bool>(settings.setValue(
                QStringLiteral("editor.tabSize"), 2, Settings::Layer::Workspace)));
            workspace.closeProject();
        }

        // Re-opening the same project restores its own overrides.
        Workspace reopened(settings, *m_scheduler);
        QVERIFY(static_cast<bool>(reopened.openProject(m_dir->path())));
        QCOMPARE(settings.intValue(QStringLiteral("editor.tabSize")), 2);
    }

    void aProjectWithNoOverridesDoesNotGetAKeysFolder()
    {
        // Creating .keys in every project the user merely opens would litter
        // their repositories and show up in git status.
        Settings settings;
        Workspace workspace(settings, *m_scheduler);
        QVERIFY(static_cast<bool>(workspace.openProject(m_dir->path())));
        workspace.closeProject();

        QVERIFY(!FileSystem::exists(path(QStringLiteral(".keys"))));
    }
};

QTEST_MAIN(ProjectTests)
#include "ProjectTests.moc"
