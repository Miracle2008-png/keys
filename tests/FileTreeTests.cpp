#include "core/TaskScheduler.h"
#include "filesystem/FileSystem.h"
#include "filesystem/FileWatcher.h"
#include "project/Project.h"
#include "workspace/FileTreeModel.h"

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace keys::workspace;
using keys::core::TaskScheduler;
using keys::fs::FileSystem;
using keys::fs::FileWatcher;
using keys::project::Project;

class FileTreeTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Project> m_project;
    std::unique_ptr<FileWatcher> m_watcher;
    std::unique_ptr<TaskScheduler> m_scheduler;
    std::unique_ptr<FileTreeModel> m_model;

    [[nodiscard]] QString path(const QString& relative) const
    {
        return QDir(m_dir->path()).filePath(relative);
    }

    void makeDirectory(const QString& relative) const
    {
        QVERIFY(static_cast<bool>(FileSystem::createDirectory(path(relative))));
    }

    void makeFile(const QString& relative, const QString& body = QStringLiteral("x")) const
    {
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(path(relative), body)));
    }

    /// Directory reads are asynchronous, so tests wait for the model to settle
    /// rather than sleeping for an arbitrary interval.
    [[nodiscard]] bool waitForIdle(int timeoutMs = 4000) const
    {
        QElapsedTimer timer;
        timer.start();
        while (m_model->isLoading() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        // One more pump so the final splice's queued signals are delivered.
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        return !m_model->isLoading();
    }

    [[nodiscard]] QStringList visibleNames() const
    {
        QStringList names;
        for (int row = 0; row < m_model->rowCount(); ++row) {
            names.append(m_model->data(m_model->index(row, 0),
                                       FileTreeModel::NameRole).toString());
        }
        return names;
    }

    [[nodiscard]] int rowOf(const QString& name) const
    {
        return visibleNames().indexOf(name);
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());

        m_project = std::make_unique<Project>();
        m_watcher = std::make_unique<FileWatcher>();
        m_scheduler = std::make_unique<TaskScheduler>();
        m_model = std::make_unique<FileTreeModel>(*m_project, *m_watcher, *m_scheduler);
    }

    void cleanup()
    {
        // Destroyed in reverse order of construction: the model holds references
        // to the other three.
        m_model.reset();
        m_scheduler.reset();
        m_watcher.reset();
        m_project.reset();
        m_dir.reset();
    }

    // ---- Model contract ---------------------------------------------------

    void satisfiesTheModelContract()
    {
        // Qt's own tester catches malformed begin/end insert and remove pairs,
        // which are the mistakes that corrupt a view silently rather than
        // crashing where they are made.
        QAbstractItemModelTester tester(m_model.get(),
                                        QAbstractItemModelTester::FailureReportingMode::Warning);

        makeDirectory(QStringLiteral("src"));
        makeFile(QStringLiteral("README.md"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        const int row = rowOf(QStringLiteral("src"));
        QVERIFY(row >= 0);
        m_model->toggleExpanded(row);
        QVERIFY(waitForIdle());
    }

    void isEmptyWithNoProject()
    {
        QCOMPARE(m_model->rowCount(), 0);
    }

    // ---- Listing ----------------------------------------------------------

    void showsTopLevelEntriesWhenAProjectOpens()
    {
        makeDirectory(QStringLiteral("src"));
        makeFile(QStringLiteral("README.md"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        // Directories first, then files - the order listDirectory guarantees.
        QCOMPARE(visibleNames(), QStringList({QStringLiteral("src"),
                                              QStringLiteral("README.md")}));
    }

    void doesNotWalkTheWholeTreeUpFront()
    {
        // Laziness is the whole reason the explorer opens quickly. A nested file
        // must not appear until its folder is expanded.
        makeDirectory(QStringLiteral("src"));
        makeFile(QStringLiteral("src/main.cpp"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QCOMPARE(m_model->rowCount(), 1);
        QVERIFY(!visibleNames().contains(QStringLiteral("main.cpp")));
    }

    void expandingRevealsChildrenAtTheNextDepth()
    {
        makeDirectory(QStringLiteral("src"));
        makeFile(QStringLiteral("src/main.cpp"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        m_model->toggleExpanded(rowOf(QStringLiteral("src")));
        QVERIFY(waitForIdle());

        QCOMPARE(visibleNames(), QStringList({QStringLiteral("src"),
                                              QStringLiteral("main.cpp")}));
        QCOMPARE(m_model->data(m_model->index(0, 0), FileTreeModel::DepthRole).toInt(), 0);
        QCOMPARE(m_model->data(m_model->index(1, 0), FileTreeModel::DepthRole).toInt(), 1);
    }

    void collapsingRemovesTheWholeSubtree()
    {
        makeDirectory(QStringLiteral("a"));
        makeDirectory(QStringLiteral("a/b"));
        makeFile(QStringLiteral("a/b/deep.txt"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        m_model->toggleExpanded(rowOf(QStringLiteral("a")));
        QVERIFY(waitForIdle());
        m_model->toggleExpanded(rowOf(QStringLiteral("b")));
        QVERIFY(waitForIdle());
        QCOMPARE(m_model->rowCount(), 3);

        // Collapsing "a" must take "b" and its file with it, not just "b".
        m_model->toggleExpanded(rowOf(QStringLiteral("a")));
        QCOMPARE(m_model->rowCount(), 1);
    }

    void ignoredPathsAreHidden()
    {
        // The project's own .gitignore is authority over what belongs in the
        // explorer; showing node_modules would bury the project's real files.
        makeFile(QStringLiteral(".gitignore"), QStringLiteral("secrets/\n*.tmp\n"));
        makeDirectory(QStringLiteral("secrets"));
        makeFile(QStringLiteral("scratch.tmp"));
        makeFile(QStringLiteral("keep.txt"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        const QStringList names = visibleNames();
        QVERIFY(!names.contains(QStringLiteral("secrets")));
        QVERIFY(!names.contains(QStringLiteral("scratch.tmp")));
        QVERIFY(names.contains(QStringLiteral("keep.txt")));
    }

    void emptyDirectoryLosesItsChevron()
    {
        // A folder is drawn expandable until proven otherwise; once read and
        // found empty, the chevron must go, or the user clicks a control that
        // does nothing.
        makeDirectory(QStringLiteral("empty"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        const int row = rowOf(QStringLiteral("empty"));
        QVERIFY(m_model->data(m_model->index(row, 0),
                              FileTreeModel::HasChildrenRole).toBool());

        m_model->toggleExpanded(row);
        QVERIFY(waitForIdle());

        QVERIFY(!m_model->data(m_model->index(row, 0),
                               FileTreeModel::HasChildrenRole).toBool());
    }

    // ---- Selection and activation -----------------------------------------

    void activatingAFileSignalsItRatherThanOpeningIt()
    {
        makeFile(QStringLiteral("main.cpp"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QSignalSpy spy(m_model.get(), &FileTreeModel::fileActivated);
        m_model->activate(rowOf(QStringLiteral("main.cpp")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), FileSystem::normalize(path(QStringLiteral("main.cpp"))));
    }

    void activatingADirectoryExpandsItAndSignalsNothing()
    {
        makeDirectory(QStringLiteral("src"));
        makeFile(QStringLiteral("src/main.cpp"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QSignalSpy spy(m_model.get(), &FileTreeModel::fileActivated);
        m_model->activate(rowOf(QStringLiteral("src")));
        QVERIFY(waitForIdle());

        QCOMPARE(spy.count(), 0);
        QVERIFY(visibleNames().contains(QStringLiteral("main.cpp")));
    }

    void selectionIsReportedPerRow()
    {
        makeFile(QStringLiteral("a.txt"));
        makeFile(QStringLiteral("b.txt"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        m_model->setSelectedPath(FileSystem::normalize(path(QStringLiteral("a.txt"))));

        QVERIFY(m_model->data(m_model->index(rowOf(QStringLiteral("a.txt")), 0),
                              FileTreeModel::IsSelectedRole).toBool());
        QVERIFY(!m_model->data(m_model->index(rowOf(QStringLiteral("b.txt")), 0),
                               FileTreeModel::IsSelectedRole).toBool());
    }

    // ---- File operations --------------------------------------------------

    void createFileAppearsInTheTree()
    {
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QVERIFY(m_model->createFile(m_dir->path(), QStringLiteral("new.txt")));
        QVERIFY(waitForIdle());

        QVERIFY(visibleNames().contains(QStringLiteral("new.txt")));
        QVERIFY(FileSystem::exists(path(QStringLiteral("new.txt"))));
    }

    void createFolderAppearsInTheTree()
    {
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QVERIFY(m_model->createFolder(m_dir->path(), QStringLiteral("lib")));
        QVERIFY(waitForIdle());

        QVERIFY(visibleNames().contains(QStringLiteral("lib")));
        QVERIFY(FileSystem::isDirectory(path(QStringLiteral("lib"))));
    }

    void namesWithASeparatorAreRejected()
    {
        // A rename must not be able to move an item somewhere else.
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QSignalSpy spy(m_model.get(), &FileTreeModel::errorOccurred);
        QVERIFY(!m_model->createFile(m_dir->path(), QStringLiteral("nested/file.txt")));
        QCOMPARE(spy.count(), 1);
        QVERIFY(!FileSystem::exists(path(QStringLiteral("nested"))));
    }

    void reservedAndEmptyNamesAreRejected()
    {
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QSignalSpy spy(m_model.get(), &FileTreeModel::errorOccurred);
        QVERIFY(!m_model->createFile(m_dir->path(), QString()));
        QVERIFY(!m_model->createFile(m_dir->path(), QStringLiteral("CON")));
        QVERIFY(!m_model->createFile(m_dir->path(), QStringLiteral("what?.txt")));
        QCOMPARE(spy.count(), 3);
    }

    void operationsOutsideTheProjectAreRefused()
    {
        // The explorer must never write outside the open project, whatever path
        // reaches the call.
        QTemporaryDir outside;
        QVERIFY(outside.isValid());

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QSignalSpy spy(m_model.get(), &FileTreeModel::errorOccurred);
        QVERIFY(!m_model->createFile(outside.path(), QStringLiteral("escape.txt")));
        QCOMPARE(spy.count(), 1);
        QVERIFY(!FileSystem::exists(QDir(outside.path()).filePath(QStringLiteral("escape.txt"))));
    }

    void renamingTheProjectRootIsRefused()
    {
        // It would leave the workspace pointing at a path that no longer exists.
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QSignalSpy spy(m_model.get(), &FileTreeModel::errorOccurred);
        QVERIFY(!m_model->rename(m_dir->path(), QStringLiteral("renamed")));
        QCOMPARE(spy.count(), 1);
    }

    void renameUpdatesTheTree()
    {
        makeFile(QStringLiteral("before.txt"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QVERIFY(m_model->rename(path(QStringLiteral("before.txt")),
                                QStringLiteral("after.txt")));
        QVERIFY(waitForIdle());

        const QStringList names = visibleNames();
        QVERIFY(names.contains(QStringLiteral("after.txt")));
        QVERIFY(!names.contains(QStringLiteral("before.txt")));
    }

    void renameOntoAnExistingNameIsRefused()
    {
        makeFile(QStringLiteral("a.txt"));
        makeFile(QStringLiteral("b.txt"), QStringLiteral("keep me"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        QSignalSpy spy(m_model.get(), &FileTreeModel::errorOccurred);
        QVERIFY(!m_model->rename(path(QStringLiteral("a.txt")), QStringLiteral("b.txt")));
        QCOMPARE(spy.count(), 1);

        // The file that would have been overwritten must be untouched.
        const auto body = FileSystem::readTextFile(path(QStringLiteral("b.txt")));
        QVERIFY(static_cast<bool>(body));
        QCOMPARE(body.value(), QStringLiteral("keep me"));
    }

    void deletingClearsASelectionInsideIt()
    {
        makeDirectory(QStringLiteral("doomed"));
        makeFile(QStringLiteral("doomed/file.txt"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());

        m_model->setSelectedPath(FileSystem::normalize(path(QStringLiteral("doomed/file.txt"))));
        QVERIFY(m_model->moveToTrash(path(QStringLiteral("doomed"))));
        QVERIFY(waitForIdle());

        // A selection pointing into a deleted folder would leave the UI
        // highlighting something that no longer exists.
        QVERIFY(m_model->selectedPath().isEmpty());
        QVERIFY(!visibleNames().contains(QStringLiteral("doomed")));
    }

    // ---- Refresh ----------------------------------------------------------

    void refreshPicksUpExternalChanges()
    {
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());
        QCOMPARE(m_model->rowCount(), 0);

        // Written behind the model's back, as another tool would.
        makeFile(QStringLiteral("appeared.txt"));
        m_model->refreshDirectory(m_dir->path());
        QVERIFY(waitForIdle());

        QVERIFY(visibleNames().contains(QStringLiteral("appeared.txt")));
    }

    void refreshKeepsFoldersOpen()
    {
        // Saving a file triggers a refresh. If that collapsed the tree, the
        // explorer would fold up every time the user pressed Ctrl+S.
        makeDirectory(QStringLiteral("src"));
        makeFile(QStringLiteral("src/main.cpp"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());
        m_model->toggleExpanded(rowOf(QStringLiteral("src")));
        QVERIFY(waitForIdle());
        QVERIFY(visibleNames().contains(QStringLiteral("main.cpp")));

        makeFile(QStringLiteral("other.txt"));
        m_model->refreshDirectory(m_dir->path());
        QVERIFY(waitForIdle());

        const QStringList names = visibleNames();
        QVERIFY(names.contains(QStringLiteral("other.txt")));
        QVERIFY2(names.contains(QStringLiteral("main.cpp")),
                 "the expanded folder collapsed on refresh");
    }

    void closingAProjectEmptiesTheTree()
    {
        makeFile(QStringLiteral("a.txt"));
        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitForIdle());
        QCOMPARE(m_model->rowCount(), 1);

        m_project->close();
        QVERIFY(waitForIdle());
        QCOMPARE(m_model->rowCount(), 0);
    }
};

QTEST_MAIN(FileTreeTests)
#include "FileTreeTests.moc"
