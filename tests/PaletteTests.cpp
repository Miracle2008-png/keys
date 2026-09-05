#include "core/CommandRegistry.h"
#include "core/TaskScheduler.h"
#include "filesystem/FileSystem.h"
#include "project/Project.h"
#include "search/FileIndex.h"
#include "ui/CommandPaletteModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using keys::core::Command;
using keys::core::CommandRegistry;
using keys::core::TaskScheduler;
using keys::fs::FileSystem;
using keys::project::Project;
using keys::search::FileIndex;
using keys::ui::CommandPaletteModel;

/// The palette model: what the box shows for a given query, in what order, and
/// what choosing a row does. The QML layer is a view over exactly these roles,
/// so pinning them here pins the behaviour the user sees.
class PaletteTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Project> m_project;
    std::unique_ptr<TaskScheduler> m_scheduler;
    std::unique_ptr<FileIndex> m_index;
    std::unique_ptr<CommandRegistry> m_commands;
    std::unique_ptr<CommandPaletteModel> m_palette;

    [[nodiscard]] QString path(const QString& relative) const
    {
        return QDir(m_dir->path()).filePath(relative);
    }

    void writeFile(const QString& relative) const
    {
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(path(relative),
                                                            QStringLiteral("x\n"))));
    }

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

    /// The value of a role on one row, by name rather than by enumerator, which
    /// is also what QML uses - so a renamed role fails here too.
    [[nodiscard]] QVariant roleAt(int row, const char* name) const
    {
        const QHash<int, QByteArray> names = m_palette->roleNames();
        for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
            if (it.value() == name) {
                return m_palette->data(m_palette->index(row, 0), it.key());
            }
        }
        return {};
    }

    [[nodiscard]] QString titleAt(int row) const
    {
        return roleAt(row, "title").toString();
    }

    [[nodiscard]] QString groupAt(int row) const
    {
        return roleAt(row, "group").toString();
    }

    /// The row index of a title, or -1. Used rather than asserting on an exact
    /// position where only relative order is the contract.
    [[nodiscard]] int rowOf(const QString& title) const
    {
        for (int row = 0; row < m_palette->count(); ++row) {
            if (titleAt(row) == title) {
                return row;
            }
        }
        return -1;
    }

    void addCommand(const QString& id, const QString& title, const QString& category,
                    const QStringList& keywords = {},
                    std::function<bool()> isEnabled = {})
    {
        Command command;
        command.id = id;
        command.title = title;
        command.category = category;
        command.keywords = keywords;
        command.handler = [this, id] { m_invoked.append(id); };
        command.isEnabled = std::move(isEnabled);
        QVERIFY(static_cast<bool>(m_commands->registerCommand(std::move(command))));
    }

    QStringList m_invoked;

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());

        m_invoked.clear();

        m_project = std::make_unique<Project>();
        m_scheduler = std::make_unique<TaskScheduler>();
        m_index = std::make_unique<FileIndex>(*m_project, *m_scheduler);
        m_commands = std::make_unique<CommandRegistry>();
        m_palette = std::make_unique<CommandPaletteModel>(*m_commands, *m_index);
    }

    void cleanup()
    {
        m_palette.reset();
        m_commands.reset();
        m_index.reset();
        m_scheduler.reset();
        m_project.reset();
        m_dir.reset();
    }

    /// Populates a project with files and waits for the index to settle, for the
    /// cases that need real files rather than commands alone.
    void openProjectWithFiles()
    {
        writeFile(QStringLiteral("main.cpp"));
        QVERIFY(QDir(m_dir->path()).mkpath(QStringLiteral("src")));
        writeFile(QStringLiteral("src/util.cpp"));
        writeFile(QStringLiteral("src/Widget.cpp"));

        QVERIFY(static_cast<bool>(m_project->open(m_dir->path())));
        QVERIFY(waitFor([this] { return !m_index->isBuilding(); }));
        QVERIFY(m_index->fileCount() >= 3);
    }

    // ---- Modes -------------------------------------------------------------

    void emptyQueryShowsFilesAndCommands()
    {
        openProjectWithFiles();
        addCommand(QStringLiteral("test.split"), QStringLiteral("Split Editor"),
                   QStringLiteral("View"));

        m_palette->setQuery(QString());

        QVERIFY(rowOf(QStringLiteral("main.cpp")) >= 0);
        QVERIFY(rowOf(QStringLiteral("Split Editor")) >= 0);
    }

    void commandPrefixHidesFiles()
    {
        openProjectWithFiles();
        addCommand(QStringLiteral("test.split"), QStringLiteral("Split Editor"),
                   QStringLiteral("View"));

        m_palette->setQuery(QStringLiteral(">"));

        QVERIFY(rowOf(QStringLiteral("Split Editor")) >= 0);
        // Not merely ranked lower: files are absent entirely in command mode.
        for (int row = 0; row < m_palette->count(); ++row) {
            QCOMPARE(groupAt(row), QStringLiteral("Commands"));
        }
    }

    void commandPrefixStripsThePrefixFromTheQuery()
    {
        addCommand(QStringLiteral("test.split"), QStringLiteral("Split Editor"),
                   QStringLiteral("View"));
        addCommand(QStringLiteral("test.save"), QStringLiteral("Save"),
                   QStringLiteral("File"));

        // If the `>` were matched literally nothing would match at all.
        m_palette->setQuery(QStringLiteral(">split"));

        QCOMPARE(m_palette->count(), 1);
        QCOMPARE(titleAt(0), QStringLiteral("Split Editor"));
    }

    void placeholderReflectsTheMode()
    {
        const QString files = m_palette->placeholder();
        m_palette->setQuery(QStringLiteral(">"));
        QVERIFY(m_palette->placeholder() != files);
    }

    // ---- Grouping and ordering --------------------------------------------

    void filesAreGroupedBeforeCommands()
    {
        openProjectWithFiles();
        addCommand(QStringLiteral("test.util"), QStringLiteral("Utility Thing"),
                   QStringLiteral("View"));

        m_palette->setQuery(QStringLiteral("util"));

        const int file = rowOf(QStringLiteral("util.cpp"));
        const int command = rowOf(QStringLiteral("Utility Thing"));
        QVERIFY(file >= 0);
        QVERIFY(command >= 0);
        // Grouped rather than interleaved: the two score on different scales, so
        // a mediocre command must not land above an exact file match.
        QVERIFY(file < command);
        QCOMPARE(groupAt(file), QStringLiteral("Files"));
        QCOMPARE(groupAt(command), QStringLiteral("Commands"));
    }

    void groupStartMarksOnlyTheFirstRowOfEachGroup()
    {
        openProjectWithFiles();
        addCommand(QStringLiteral("test.a"), QStringLiteral("Alpha"),
                   QStringLiteral("View"));
        addCommand(QStringLiteral("test.b"), QStringLiteral("Beta"),
                   QStringLiteral("View"));

        m_palette->setQuery(QString());
        QVERIFY(m_palette->count() > 2);

        int starts = 0;
        for (int row = 0; row < m_palette->count(); ++row) {
            if (roleAt(row, "isGroupStart").toBool()) {
                ++starts;
                QVERIFY(row == 0 || groupAt(row) != groupAt(row - 1));
            }
        }
        // One heading per group, and exactly two groups are present.
        QCOMPARE(starts, 2);
    }

    void betterMatchesRankFirst()
    {
        addCommand(QStringLiteral("test.split"), QStringLiteral("Split Editor"),
                   QStringLiteral("View"));
        addCommand(QStringLiteral("test.spell"), QStringLiteral("Toggle Spell Check"),
                   QStringLiteral("Editor"));

        m_palette->setQuery(QStringLiteral(">spl"));

        QVERIFY(m_palette->count() >= 2);
        QCOMPARE(titleAt(0), QStringLiteral("Split Editor"));
    }

    void matchesAgainstTheCategoryToo()
    {
        addCommand(QStringLiteral("test.split"), QStringLiteral("Split Editor"),
                   QStringLiteral("View"));

        m_palette->setQuery(QStringLiteral(">view split"));
        QCOMPARE(m_palette->count(), 1);
    }

    void matchesKeywordsWhenTheTitleDoesNot()
    {
        addCommand(QStringLiteral("test.open"), QStringLiteral("Open Project"),
                   QStringLiteral("File"), {QStringLiteral("folder")});

        m_palette->setQuery(QStringLiteral(">folder"));
        QCOMPARE(m_palette->count(), 1);
        QCOMPARE(titleAt(0), QStringLiteral("Open Project"));
    }

    void keywordMatchesRankBelowDirectOnes()
    {
        addCommand(QStringLiteral("test.direct"), QStringLiteral("Folder View"),
                   QStringLiteral("View"));
        addCommand(QStringLiteral("test.keyword"), QStringLiteral("Open Project"),
                   QStringLiteral("File"), {QStringLiteral("folder")});

        m_palette->setQuery(QStringLiteral(">folder"));

        QCOMPARE(m_palette->count(), 2);
        QCOMPARE(titleAt(0), QStringLiteral("Folder View"));
    }

    // ---- Highlight positions ----------------------------------------------

    void matchPositionsAreRelativeToTheTitle()
    {
        addCommand(QStringLiteral("test.split"), QStringLiteral("Split Editor"),
                   QStringLiteral("View"));

        m_palette->setQuery(QStringLiteral(">spl"));
        QCOMPARE(m_palette->count(), 1);

        const QVariantList positions = roleAt(0, "matchPositions").toList();
        QCOMPARE(positions.size(), 3);
        // "Split Editor" - matched at the start of the title, not shifted by the
        // "View " category prefix the scorer actually saw.
        QCOMPARE(positions.at(0).toInt(), 0);
        QCOMPARE(positions.at(1).toInt(), 1);
        QCOMPARE(positions.at(2).toInt(), 2);
    }

    void positionsInsideTheCategoryAreDropped()
    {
        addCommand(QStringLiteral("test.split"), QStringLiteral("Split Editor"),
                   QStringLiteral("View"));

        m_palette->setQuery(QStringLiteral(">view"));
        QCOMPARE(m_palette->count(), 1);

        // Everything matched inside the category, which QML does not highlight.
        QVERIFY(roleAt(0, "matchPositions").toList().isEmpty());
    }

    void filePositionsAreRelativeToTheFileName()
    {
        openProjectWithFiles();

        m_palette->setQuery(QStringLiteral("util"));
        const int row = rowOf(QStringLiteral("util.cpp"));
        QVERIFY(row >= 0);

        const QVariantList positions = roleAt(row, "matchPositions").toList();
        QCOMPARE(positions.size(), 4);
        QCOMPARE(positions.at(0).toInt(), 0);   // not 4, which is where "src/" ends
        QCOMPARE(roleAt(row, "subtitle").toString(), QStringLiteral("src"));
    }

    // ---- Selection ---------------------------------------------------------

    void firstRowIsSelectedAfterEveryQuery()
    {
        addCommand(QStringLiteral("test.a"), QStringLiteral("Alpha"),
                   QStringLiteral("View"));
        addCommand(QStringLiteral("test.b"), QStringLiteral("Beta"),
                   QStringLiteral("View"));

        m_palette->setQuery(QStringLiteral(">"));
        m_palette->selectNext();
        QCOMPARE(m_palette->selectedIndex(), 1);

        // A keystroke reorders the list, so the old selection refers to a row
        // that may no longer exist: it resets rather than following.
        m_palette->setQuery(QStringLiteral(">a"));
        QCOMPARE(m_palette->selectedIndex(), 0);
    }

    void selectionWrapsAtBothEnds()
    {
        addCommand(QStringLiteral("test.a"), QStringLiteral("Alpha"),
                   QStringLiteral("View"));
        addCommand(QStringLiteral("test.b"), QStringLiteral("Beta"),
                   QStringLiteral("View"));
        m_palette->setQuery(QStringLiteral(">"));
        QCOMPARE(m_palette->count(), 2);

        m_palette->selectPrevious();
        QCOMPARE(m_palette->selectedIndex(), 1);
        m_palette->selectNext();
        QCOMPARE(m_palette->selectedIndex(), 0);
    }

    void selectionIsClampedRatherThanOutOfRange()
    {
        addCommand(QStringLiteral("test.a"), QStringLiteral("Alpha"),
                   QStringLiteral("View"));
        m_palette->setQuery(QStringLiteral(">"));

        m_palette->setSelectedIndex(99);
        QCOMPARE(m_palette->selectedIndex(), 0);
        m_palette->setSelectedIndex(-5);
        QCOMPARE(m_palette->selectedIndex(), 0);
    }

    void navigatingAnEmptyListIsHarmless()
    {
        m_palette->setQuery(QStringLiteral(">nothingmatchesthis"));
        QCOMPARE(m_palette->count(), 0);

        m_palette->selectNext();
        m_palette->selectPrevious();
        QVERIFY(!m_palette->acceptSelected());
    }

    // ---- Accepting ---------------------------------------------------------

    void acceptingACommandInvokesIt()
    {
        addCommand(QStringLiteral("test.split"), QStringLiteral("Split Editor"),
                   QStringLiteral("View"));
        m_palette->setQuery(QStringLiteral(">split"));

        QSignalSpy accepted(m_palette.get(), &CommandPaletteModel::accepted);
        QVERIFY(m_palette->acceptSelected());
        QCOMPARE(m_invoked, QStringList{QStringLiteral("test.split")});
        QCOMPARE(accepted.count(), 1);
    }

    void acceptingAFileReportsItRatherThanOpeningIt()
    {
        openProjectWithFiles();
        m_palette->setQuery(QStringLiteral("util"));
        QCOMPARE(m_palette->selectedIndex(), rowOf(QStringLiteral("util.cpp")));

        QSignalSpy chosen(m_palette.get(), &CommandPaletteModel::fileChosen);
        QVERIFY(m_palette->acceptSelected());
        QCOMPARE(chosen.count(), 1);
        // The palette does not open files itself; the workspace does.
        QCOMPARE(chosen.at(0).at(0).toString(), QStringLiteral("src/util.cpp"));
    }

    void disabledCommandsAreShownButDoNothing()
    {
        addCommand(QStringLiteral("test.save"), QStringLiteral("Save"),
                   QStringLiteral("File"), {}, [] { return false; });
        m_palette->setQuery(QStringLiteral(">save"));

        QCOMPARE(m_palette->count(), 1);
        // Visible, so the list does not shift under the user...
        QCOMPARE(roleAt(0, "enabled").toBool(), false);
        // ...but choosing it is a no-op rather than a failure.
        QVERIFY(!m_palette->acceptSelected());
        QVERIFY(m_invoked.isEmpty());
    }

    // ---- Staying current ---------------------------------------------------

    void resetClearsTheQuery()
    {
        addCommand(QStringLiteral("test.a"), QStringLiteral("Alpha"),
                   QStringLiteral("View"));
        m_palette->setQuery(QStringLiteral(">zzz"));
        QCOMPARE(m_palette->count(), 0);

        m_palette->reset();
        QVERIFY(m_palette->query().isEmpty());
        QCOMPARE(m_palette->count(), 1);
    }

    void aCommandRegisteredWhileOpenAppears()
    {
        m_palette->setQuery(QStringLiteral(">alpha"));
        QCOMPARE(m_palette->count(), 0);

        // A module can register at any time, and the index finishes building
        // after the palette may already be open. Both rebuild.
        addCommand(QStringLiteral("test.a"), QStringLiteral("Alpha"),
                   QStringLiteral("View"));
        QCOMPARE(m_palette->count(), 1);
    }

    void anIndexedFileAppearsWhenIndexingFinishes()
    {
        m_palette->setQuery(QStringLiteral("widget"));
        QCOMPARE(m_palette->count(), 0);

        openProjectWithFiles();
        QCOMPARE(m_palette->count(), 1);
        QCOMPARE(titleAt(0), QStringLiteral("Widget.cpp"));
    }
};

QTEST_MAIN(PaletteTests)
#include "PaletteTests.moc"
