#include "workspace/EditorGroup.h"
#include "workspace/EditorLayout.h"

#include <QSignalSpy>
#include <QTest>

using namespace keys::workspace;
using keys::editor::TextDocument;

class EditorLayoutTests : public QObject {
    Q_OBJECT

private slots:
    // ---- Group: opening ---------------------------------------------------

    void opensATabAndActivatesIt()
    {
        EditorGroup group;
        QSignalSpy spy(&group, &EditorGroup::tabsChanged);

        TextDocument* document =
            group.openFile(QStringLiteral("/p/a.txt"), QStringLiteral("body"));

        QVERIFY(document);
        QCOMPARE(group.tabCount(), 1);
        QCOMPARE(group.activeIndex(), 0);
        QCOMPARE(group.activeDocument(), document);
        QCOMPARE(document->text(), QStringLiteral("body"));
        QCOMPARE(spy.count(), 1);
    }

    void openingAnAlreadyOpenFileActivatesItInstead()
    {
        // Two tabs for one file in the same pane would give the user two carets
        // and two undo stacks over the same bytes, and no way to tell which one
        // saves.
        EditorGroup group;
        TextDocument* first =
            group.openFile(QStringLiteral("/p/a.txt"), QStringLiteral("one"));
        group.openFile(QStringLiteral("/p/b.txt"), QStringLiteral("two"));
        QCOMPARE(group.tabCount(), 2);

        TextDocument* again =
            group.openFile(QStringLiteral("/p/a.txt"), QStringLiteral("ignored"));

        QCOMPARE(group.tabCount(), 2);
        QCOMPARE(again, first);
        QCOMPARE(group.activeIndex(), 0);
        // The existing document is untouched: re-opening must not discard edits.
        QCOMPARE(again->text(), QStringLiteral("one"));
    }

    void newTabsOpenAfterTheActiveOne()
    {
        // Opening a file while working keeps it beside what it relates to,
        // rather than at the far right of a long bar.
        EditorGroup group;
        group.openFile(QStringLiteral("/p/a.txt"), QString());
        group.openFile(QStringLiteral("/p/b.txt"), QString());
        group.setActiveIndex(0);

        group.openFile(QStringLiteral("/p/c.txt"), QString());

        QCOMPARE(group.activeIndex(), 1);
        QCOMPARE(group.documentAt(1)->path(), QStringLiteral("/p/c.txt"));
        QCOMPARE(group.documentAt(2)->path(), QStringLiteral("/p/b.txt"));
    }

    // ---- Group: closing ---------------------------------------------------

    void closingActivatesTheNeighbour()
    {
        // Falling back to index 0 would jump the user across the bar.
        EditorGroup group;
        for (const char* name : {"/p/a", "/p/b", "/p/c"}) {
            group.openFile(QString::fromLatin1(name), QString());
        }
        group.setActiveIndex(1);

        QVERIFY(group.closeTab(1));
        QCOMPARE(group.tabCount(), 2);
        // The tab that slid into position 1 is now active.
        QCOMPARE(group.activeIndex(), 1);
        QCOMPARE(group.activeDocument()->path(), QStringLiteral("/p/c"));
    }

    void closingTheLastTabActivatesTheOneBefore()
    {
        EditorGroup group;
        group.openFile(QStringLiteral("/p/a"), QString());
        group.openFile(QStringLiteral("/p/b"), QString());
        group.setActiveIndex(1);

        QVERIFY(group.closeTab(1));
        QCOMPARE(group.activeIndex(), 0);
    }

    void closingBeforeTheActiveTabKeepsItActive()
    {
        EditorGroup group;
        for (const char* name : {"/p/a", "/p/b", "/p/c"}) {
            group.openFile(QString::fromLatin1(name), QString());
        }
        group.setActiveIndex(2);

        QVERIFY(group.closeTab(0));
        // Still showing "c", now at index 1.
        QCOMPARE(group.activeIndex(), 1);
        QCOMPARE(group.activeDocument()->path(), QStringLiteral("/p/c"));
    }

    void closingTheOnlyTabEmptiesTheGroup()
    {
        EditorGroup group;
        group.openFile(QStringLiteral("/p/a"), QString());

        QVERIFY(group.closeTab(0));
        QVERIFY(group.isEmpty());
        QCOMPARE(group.activeIndex(), -1);
        QVERIFY(!group.activeDocument());
    }

    void closingAnInvalidIndexFails()
    {
        EditorGroup group;
        QVERIFY(!group.closeTab(0));
        QVERIFY(!group.closeTab(-1));
    }

    // ---- Group: reordering ------------------------------------------------

    void movingATabKeepsTheSameDocumentActive()
    {
        // Dragging a tab must not change which file the user is looking at.
        EditorGroup group;
        for (const char* name : {"/p/a", "/p/b", "/p/c"}) {
            group.openFile(QString::fromLatin1(name), QString());
        }
        group.setActiveIndex(0);

        QVERIFY(group.moveTab(0, 2));

        QCOMPARE(group.documentAt(2)->path(), QStringLiteral("/p/a"));
        QCOMPARE(group.activeIndex(), 2);
        QCOMPARE(group.activeDocument()->path(), QStringLiteral("/p/a"));
    }

    void movingToTheSamePlaceIsANoOp()
    {
        EditorGroup group;
        group.openFile(QStringLiteral("/p/a"), QString());
        QVERIFY(!group.moveTab(0, 0));
    }

    void movingOutOfRangeFails()
    {
        EditorGroup group;
        group.openFile(QStringLiteral("/p/a"), QString());
        QVERIFY(!group.moveTab(0, 5));
        QVERIFY(!group.moveTab(-1, 0));
    }

    // ---- Group: dirty state -----------------------------------------------

    void reportsUnsavedChanges()
    {
        EditorGroup group;
        TextDocument* document =
            group.openFile(QStringLiteral("/p/a"), QStringLiteral("clean"));
        QVERIFY(!group.hasUnsavedChanges());

        document->insertText(QStringLiteral("!"));
        QVERIFY(group.hasUnsavedChanges());
    }

    void aTabsModifiedSignalCarriesItsCurrentIndex()
    {
        // The tab bar addresses tabs by index, and indices shift as tabs move.
        // Looking the index up at emit time is what keeps the signal correct
        // after a reorder.
        EditorGroup group;
        group.openFile(QStringLiteral("/p/a"), QString());
        TextDocument* second = group.openFile(QStringLiteral("/p/b"), QString());

        group.moveTab(1, 0);   // "b" is now index 0

        QSignalSpy spy(&group, &EditorGroup::tabModifiedChanged);
        second->insertText(QStringLiteral("x"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
    }

    // ---- Layout: splitting ------------------------------------------------

    void startsWithOneGroup()
    {
        // An editor with no panes has nowhere to open a file.
        const EditorLayout layout;
        QCOMPARE(layout.groupCount(), 1);
        QVERIFY(layout.activeGroup());
        QCOMPARE(layout.activeGroupIndex(), 0);
    }

    void splittingCarriesTheCurrentFileIntoTheNewPane()
    {
        // Splitting to an empty pane would make the user re-open the file they
        // were already looking at.
        EditorLayout layout;
        layout.openInActiveGroup(QStringLiteral("/p/a.txt"), QStringLiteral("shared"));

        EditorGroup* second = layout.split();

        QVERIFY(second);
        QCOMPARE(layout.groupCount(), 2);
        QCOMPARE(layout.activeGroupIndex(), 1);
        QCOMPARE(second->tabCount(), 1);
        QCOMPARE(second->activeDocument()->text(), QStringLiteral("shared"));
    }

    void eachPaneGetsItsOwnDocument()
    {
        // Independent carets and undo stacks are the point of a split; a shared
        // document would need two carets reconciled over one buffer.
        EditorLayout layout;
        layout.openInActiveGroup(QStringLiteral("/p/a.txt"), QStringLiteral("base"));
        layout.split();

        TextDocument* first = layout.groupAt(0)->activeDocument();
        TextDocument* second = layout.groupAt(1)->activeDocument();

        QVERIFY(first != second);

        second->insertText(QStringLiteral("edited "));
        QCOMPARE(first->text(), QStringLiteral("base"));
        QVERIFY(second->text().startsWith(QStringLiteral("edited")));
    }

    void splittingStopsAtTheMaximum()
    {
        // More than two panes makes each too narrow to read code in.
        EditorLayout layout;
        QVERIFY(layout.split());
        QVERIFY(!layout.split());
        QCOMPARE(layout.groupCount(), EditorLayout::kMaxGroups);
    }

    void closingASplitMovesFocusToTheSurvivor()
    {
        EditorLayout layout;
        layout.split();
        QCOMPARE(layout.activeGroupIndex(), 1);

        QVERIFY(layout.closeGroup(1));
        QCOMPARE(layout.groupCount(), 1);
        QCOMPARE(layout.activeGroupIndex(), 0);
    }

    void theLastGroupCannotBeClosed()
    {
        EditorLayout layout;
        QVERIFY(!layout.closeGroup(0));
        QCOMPARE(layout.groupCount(), 1);
    }

    void openingTargetsTheActiveGroup()
    {
        // Every command that acts on "the editor" acts on the focused pane, so
        // the user's attention picks the target.
        EditorLayout layout;
        layout.split();
        layout.setActiveGroup(0);

        layout.openInActiveGroup(QStringLiteral("/p/only-here.txt"), QString());

        QCOMPARE(layout.groupAt(0)->indexOfPath(QStringLiteral("/p/only-here.txt")), 0);
        QCOMPARE(layout.groupAt(1)->indexOfPath(QStringLiteral("/p/only-here.txt")), -1);
    }

    void resetCollapsesToOneEmptyGroup()
    {
        EditorLayout layout;
        layout.openInActiveGroup(QStringLiteral("/p/a"), QString());
        layout.split();

        layout.reset();

        QCOMPARE(layout.groupCount(), 1);
        QVERIFY(layout.groupAt(0)->isEmpty());
        QCOMPARE(layout.activeGroupIndex(), 0);
        QVERIFY(!layout.activeDocument());
    }

    void reportsUnsavedWorkInAnyPane()
    {
        EditorLayout layout;
        layout.openInActiveGroup(QStringLiteral("/p/a"), QStringLiteral("x"));
        layout.split();
        QVERIFY(!layout.hasUnsavedChanges());

        // Dirty the pane that is not focused.
        layout.setActiveGroup(0);
        layout.groupAt(1)->activeDocument()->insertText(QStringLiteral("!"));

        QVERIFY(layout.hasUnsavedChanges());
    }

    void switchingPanesReportsANewActiveDocument()
    {
        EditorLayout layout;
        layout.openInActiveGroup(QStringLiteral("/p/a"), QString());
        layout.split();

        QSignalSpy spy(&layout, &EditorLayout::activeDocumentChanged);
        layout.setActiveGroup(0);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(layout.activeDocument(), layout.groupAt(0)->activeDocument());
    }
};

QTEST_MAIN(EditorLayoutTests)
#include "EditorLayoutTests.moc"
