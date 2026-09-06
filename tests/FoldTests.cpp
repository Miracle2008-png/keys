#include "editor/FoldModel.h"
#include "editor/TextDocument.h"

#include <QTest>

using namespace keys::editor;

/// Code folding.
///
/// The mapping between visible rows and document lines is where this goes
/// wrong: every line the view draws has to resolve to the right line of the
/// file, and an off-by-one there shows the wrong text under the wrong number.
class FoldTests : public QObject {
    Q_OBJECT

private:
    TextDocument m_document;
    FoldModel m_folds;

    void load(const QString& text)
    {
        m_document.setText(text);
        m_folds.clear();
        m_folds.rebuild(m_document);
    }

private slots:
    void init() { m_document.setText(QString()); }

    // ---- Finding regions ---------------------------------------------------

    void findsAnIndentedBlock()
    {
        load(QStringLiteral("void f()\n    a;\n    b;\nvoid g()"));

        const FoldRegion* region = m_folds.regionAt(0);
        QVERIFY(region != nullptr);
        QCOMPARE(region->endLine, 2);
    }

    void findsNestedBlocks()
    {
        load(QStringLiteral("outer\n    middle\n        inner\n        inner2\n"
                            "    middle2\ndone"));

        QVERIFY(m_folds.regionAt(0) != nullptr);
        QCOMPARE(m_folds.regionAt(0)->endLine, 4);

        QVERIFY(m_folds.regionAt(1) != nullptr);
        QCOMPARE(m_folds.regionAt(1)->endLine, 3);
    }

    void aBlankLineDoesNotEndABlock()
    {
        // A blank line between two statements inside a function is part of the
        // function, not the end of it.
        load(QStringLiteral("void f()\n    a;\n\n    b;\ndone"));

        const FoldRegion* region = m_folds.regionAt(0);
        QVERIFY(region != nullptr);
        QCOMPARE(region->endLine, 3);
    }

    void aFlatFileHasNothingToFold()
    {
        load(QStringLiteral("one\ntwo\nthree"));
        QCOMPARE(static_cast<int>(m_folds.regions().size()), 0);
    }

    // ---- Folding -----------------------------------------------------------

    void foldingHidesTheBlockButNotItsHeader()
    {
        // The line carrying the marker stays visible: it is what the user
        // clicks to unfold, and it says what was folded away.
        load(QStringLiteral("void f()\n    a;\n    b;\ndone"));
        m_folds.toggle(0);

        QVERIFY(!m_folds.isHidden(0));
        QVERIFY(m_folds.isHidden(1));
        QVERIFY(m_folds.isHidden(2));
        QVERIFY(!m_folds.isHidden(3));
    }

    void togglingTwiceRestores()
    {
        load(QStringLiteral("void f()\n    a;\ndone"));
        m_folds.toggle(0);
        QVERIFY(m_folds.isFolded(0));

        m_folds.toggle(0);
        QVERIFY(!m_folds.isFolded(0));
        QVERIFY(!m_folds.isHidden(1));
    }

    void togglingALineWithNoRegionDoesNothing()
    {
        load(QStringLiteral("flat\nfile"));
        m_folds.toggle(0);
        QVERIFY(!m_folds.isFolded(0));
    }

    // ---- Visible rows ------------------------------------------------------

    void visibleCountDropsByWhatIsHidden()
    {
        load(QStringLiteral("void f()\n    a;\n    b;\ndone"));
        QCOMPARE(m_folds.visibleLineCount(4), 4);

        m_folds.toggle(0);
        QCOMPARE(m_folds.visibleLineCount(4), 2);
    }

    void visibleRowsMapToTheRightDocumentLines()
    {
        // Row 1 must be the line after the fold, not the line after the header.
        load(QStringLiteral("void f()\n    a;\n    b;\ndone"));
        m_folds.toggle(0);

        QCOMPARE(m_folds.documentLineFor(0, 4), 0);
        QCOMPARE(m_folds.documentLineFor(1, 4), 3);
    }

    void unfoldedRowsMapStraightThrough()
    {
        // The ordinary case, which must cost nothing and be exactly the
        // identity - the editor spends nearly all its time here.
        load(QStringLiteral("a\nb\nc\nd"));
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(m_folds.documentLineFor(i, 4), i);
        }
        QCOMPARE(m_folds.visibleLineCount(4), 4);
    }

    void nestedFoldsBothHide()
    {
        load(QStringLiteral("outer\n    middle\n        inner\ndone"));
        m_folds.toggle(1);

        QVERIFY(m_folds.isHidden(2));
        QCOMPARE(m_folds.visibleLineCount(4), 3);
    }

    // ---- Rebuilding --------------------------------------------------------

    void aRebuildKeepsWhatIsStillFoldable()
    {
        // Editing elsewhere in the file must not spring every fold open.
        load(QStringLiteral("void f()\n    a;\n    b;\ndone"));
        m_folds.toggle(0);
        QVERIFY(m_folds.isFolded(0));

        m_folds.rebuild(m_document);
        QVERIFY(m_folds.isFolded(0));
    }

    void aRebuildDropsRegionsThatAreGone()
    {
        load(QStringLiteral("void f()\n    a;\ndone"));
        m_folds.toggle(0);
        QVERIFY(m_folds.isFolded(0));

        // The block is gone; the fold cannot survive it.
        m_document.setText(QStringLiteral("flat\nfile\nnow"));
        m_folds.rebuild(m_document);

        QVERIFY(!m_folds.isFolded(0));
        QCOMPARE(m_folds.visibleLineCount(3), 3);
    }

    void foldAllAndUnfoldAll()
    {
        load(QStringLiteral("a\n    b\nc\n    d\ne"));
        m_folds.foldAll();
        QVERIFY(m_folds.isHidden(1));
        QVERIFY(m_folds.isHidden(3));

        m_folds.unfoldAll();
        QVERIFY(!m_folds.isHidden(1));
        QVERIFY(!m_folds.isHidden(3));
    }
};

QTEST_MAIN(FoldTests)
#include "FoldTests.moc"
