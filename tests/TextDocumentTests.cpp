#include "editor/TextDocument.h"

#include <QSignalSpy>
#include <QTest>

using namespace keys::editor;

class TextDocumentTests : public QObject {
    Q_OBJECT

private:
    TextDocument m_document;

    void type(const QString& text)
    {
        // One character at a time, as a keyboard produces them - which is what
        // exercises undo coalescing rather than bypassing it.
        for (const QChar character : text) {
            m_document.insertText(QString(character));
        }
    }

private slots:
    void init()
    {
        m_document.setText(QString());
    }

    // ---- Editing ----------------------------------------------------------

    void insertsAtTheCaret()
    {
        m_document.insertText(QStringLiteral("hello"));
        QCOMPARE(m_document.text(), QStringLiteral("hello"));
        QCOMPARE(m_document.cursor().position, (Position{0, 5}));
    }

    void insertingOverASelectionReplacesIt()
    {
        m_document.setText(QStringLiteral("keep REPLACE keep"));
        m_document.setCursorPosition(Position{0, 5});
        m_document.setCursorPosition(Position{0, 12}, true);

        m_document.insertText(QStringLiteral("new"));
        QCOMPARE(m_document.text(), QStringLiteral("keep new keep"));
    }

    void backspaceRemovesTheCharacterBefore()
    {
        m_document.setText(QStringLiteral("abc"));
        m_document.moveToDocumentEnd();
        m_document.deleteBackward();
        QCOMPARE(m_document.text(), QStringLiteral("ab"));
    }

    void backspaceAtLineStartJoinsLines()
    {
        m_document.setText(QStringLiteral("one\ntwo"));
        m_document.setCursorPosition(Position{1, 0});
        m_document.deleteBackward();

        QCOMPARE(m_document.text(), QStringLiteral("onetwo"));
        QCOMPARE(m_document.cursor().position, (Position{0, 3}));
    }

    void backspaceAtDocumentStartDoesNothing()
    {
        m_document.setText(QStringLiteral("abc"));
        m_document.setCursorPosition(Position{0, 0});
        m_document.deleteBackward();
        QCOMPARE(m_document.text(), QStringLiteral("abc"));
    }

    void deleteRemovesTheCharacterAfter()
    {
        m_document.setText(QStringLiteral("abc"));
        m_document.setCursorPosition(Position{0, 0});
        m_document.deleteForward();
        QCOMPARE(m_document.text(), QStringLiteral("bc"));
    }

    void deleteAtDocumentEndDoesNothing()
    {
        m_document.setText(QStringLiteral("abc"));
        m_document.moveToDocumentEnd();
        m_document.deleteForward();
        QCOMPARE(m_document.text(), QStringLiteral("abc"));
    }

    void backspaceWithASelectionRemovesTheSelection()
    {
        m_document.setText(QStringLiteral("abcdef"));
        m_document.setCursorPosition(Position{0, 1});
        m_document.setCursorPosition(Position{0, 4}, true);
        m_document.deleteBackward();
        QCOMPARE(m_document.text(), QStringLiteral("aef"));
    }

    // ---- Modified state ---------------------------------------------------

    void loadingTextLeavesTheDocumentClean()
    {
        m_document.setText(QStringLiteral("from disk"));
        QVERIFY(!m_document.isModified());
    }

    void editingMarksItModified()
    {
        QSignalSpy spy(&m_document, &TextDocument::modifiedChanged);
        m_document.insertText(QStringLiteral("x"));

        QVERIFY(m_document.isModified());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void savingClearsModified()
    {
        m_document.insertText(QStringLiteral("x"));
        m_document.markSaved();
        QVERIFY(!m_document.isModified());
    }

    void undoingBackToTheSavedStateClearsModified()
    {
        // A user who types and then undoes is back at the saved file, so the
        // tab's dirty dot must disappear. Tracking a boolean instead of a save
        // point would leave it stuck on.
        m_document.setText(QStringLiteral("saved"));
        m_document.markSaved();

        m_document.insertText(QStringLiteral("!"));
        QVERIFY(m_document.isModified());

        m_document.undo();
        QCOMPARE(m_document.text(), QStringLiteral("saved"));
        QVERIFY(!m_document.isModified());
    }

    // ---- Undo and redo ----------------------------------------------------

    void undoRestoresPreviousText()
    {
        m_document.setText(QStringLiteral("original"));
        m_document.moveToDocumentEnd();
        m_document.insertText(QStringLiteral(" extra"));

        QVERIFY(m_document.undo());
        QCOMPARE(m_document.text(), QStringLiteral("original"));
    }

    void redoReappliesIt()
    {
        m_document.setText(QStringLiteral("original"));
        m_document.moveToDocumentEnd();
        m_document.insertText(QStringLiteral(" extra"));
        m_document.undo();

        QVERIFY(m_document.redo());
        QCOMPARE(m_document.text(), QStringLiteral("original extra"));
    }

    void undoRestoresTheCaret()
    {
        // An undo that fixes the text but leaves the caret elsewhere makes the
        // user hunt for their place.
        m_document.setText(QStringLiteral("one\ntwo\nthree"));
        m_document.setCursorPosition(Position{1, 3});
        m_document.insertText(QStringLiteral("!"));

        m_document.undo();
        QCOMPARE(m_document.cursor().position, (Position{1, 3}));
    }

    void undoWithNoHistoryReportsFailure()
    {
        QVERIFY(!m_document.undo());
        QVERIFY(!m_document.canUndo());
    }

    void typingCoalescesIntoOneUndoStep()
    {
        // Per-keystroke undo is unusable: undoing a sentence would take a
        // hundred presses.
        m_document.setText(QString());
        type(QStringLiteral("hello"));
        QCOMPARE(m_document.text(), QStringLiteral("hello"));

        QVERIFY(m_document.undo());
        QCOMPARE(m_document.text(), QString());
    }

    void anEnterKeyBreaksTheUndoRun()
    {
        // A newline ends the thought; undoing back across several lines at once
        // is more surprising than useful.
        m_document.setText(QString());
        type(QStringLiteral("first"));
        m_document.insertText(QStringLiteral("\n"));
        type(QStringLiteral("second"));

        m_document.undo();
        QCOMPARE(m_document.text(), QStringLiteral("first\n"));

        m_document.undo();
        QCOMPARE(m_document.text(), QStringLiteral("first"));
    }

    void movingTheCaretBreaksTheUndoRun()
    {
        // Text typed somewhere else is a separate action.
        m_document.setText(QStringLiteral("ab"));
        m_document.setCursorPosition(Position{0, 2});
        type(QStringLiteral("XY"));

        m_document.setCursorPosition(Position{0, 0});
        type(QStringLiteral("Z"));

        m_document.undo();
        QCOMPARE(m_document.text(), QStringLiteral("abXY"));
    }

    void aPasteIsItsOwnUndoStep()
    {
        // A multi-character insertion is one deliberate action and must not
        // join whatever was being typed before it.
        m_document.setText(QString());
        type(QStringLiteral("abc"));
        m_document.insertText(QStringLiteral("PASTED"));

        m_document.undo();
        QCOMPARE(m_document.text(), QStringLiteral("abc"));
    }

    void deletionDoesNotMergeIntoTyping()
    {
        m_document.setText(QString());
        type(QStringLiteral("abcd"));
        m_document.deleteBackward();
        QCOMPARE(m_document.text(), QStringLiteral("abc"));

        // One undo restores the deleted character, not the whole word.
        m_document.undo();
        QCOMPARE(m_document.text(), QStringLiteral("abcd"));
    }

    void editingAfterUndoDiscardsTheRedoBranch()
    {
        m_document.setText(QStringLiteral("base"));
        m_document.moveToDocumentEnd();
        m_document.insertText(QStringLiteral(" one"));
        m_document.undo();

        m_document.moveToDocumentEnd();
        m_document.insertText(QStringLiteral(" two"));

        QVERIFY(!m_document.canRedo());
        QCOMPARE(m_document.text(), QStringLiteral("base two"));
    }

    void repeatedUndoUnwindsEverything()
    {
        m_document.setText(QStringLiteral("start"));
        m_document.moveToDocumentEnd();
        m_document.insertText(QStringLiteral(" A"));
        m_document.insertText(QStringLiteral(" B"));
        m_document.insertText(QStringLiteral(" C"));

        while (m_document.canUndo()) {
            m_document.undo();
        }
        QCOMPARE(m_document.text(), QStringLiteral("start"));
    }

    // ---- Cursor movement --------------------------------------------------

    void horizontalMovementCrossesLines()
    {
        m_document.setText(QStringLiteral("ab\ncd"));
        m_document.setCursorPosition(Position{0, 2});

        m_document.moveRight();
        QCOMPARE(m_document.cursor().position, (Position{1, 0}));

        m_document.moveLeft();
        QCOMPARE(m_document.cursor().position, (Position{0, 2}));
    }

    void movementStopsAtTheDocumentEdges()
    {
        m_document.setText(QStringLiteral("ab"));
        m_document.setCursorPosition(Position{0, 0});
        m_document.moveLeft();
        QCOMPARE(m_document.cursor().position, (Position{0, 0}));

        m_document.moveToDocumentEnd();
        m_document.moveRight();
        QCOMPARE(m_document.cursor().position, (Position{0, 2}));
    }

    void verticalMovementRemembersTheDesiredColumn()
    {
        // Passing through a short line must not drag the caret permanently
        // left - the single most noticeable cursor bug an editor can have.
        m_document.setText(QStringLiteral("aaaaaaaaaa\nbb\ncccccccccc"));
        m_document.setCursorPosition(Position{0, 8});

        m_document.moveDown();
        QCOMPARE(m_document.cursor().position, (Position{1, 2}));  // clamped

        m_document.moveDown();
        QCOMPARE(m_document.cursor().position, (Position{2, 8}));  // restored
    }

    void horizontalMovementResetsTheDesiredColumn()
    {
        m_document.setText(QStringLiteral("aaaaaaaaaa\nbb\ncccccccccc"));
        m_document.setCursorPosition(Position{0, 8});
        m_document.moveDown();

        // A left press expresses a new horizontal intent.
        m_document.moveLeft();
        m_document.moveDown();
        QCOMPARE(m_document.cursor().position, (Position{2, 1}));
    }

    void homeTogglesBetweenIndentAndColumnZero()
    {
        // On indented code, the first non-blank is almost always what is meant.
        m_document.setText(QStringLiteral("    indented"));
        m_document.setCursorPosition(Position{0, 9});

        m_document.moveToLineStart();
        QCOMPARE(m_document.cursor().position, (Position{0, 4}));

        m_document.moveToLineStart();
        QCOMPARE(m_document.cursor().position, (Position{0, 0}));
    }

    void endGoesToTheEndOfTheLine()
    {
        m_document.setText(QStringLiteral("abc\ndefgh"));
        m_document.setCursorPosition(Position{1, 1});
        m_document.moveToLineEnd();
        QCOMPARE(m_document.cursor().position, (Position{1, 5}));
    }

    // ---- Word movement ----------------------------------------------------

    void wordRightStopsAtWordStarts()
    {
        m_document.setText(QStringLiteral("alpha beta gamma"));
        m_document.setCursorPosition(Position{0, 0});

        m_document.moveWordRight();
        QCOMPARE(m_document.cursor().position, (Position{0, 6}));

        m_document.moveWordRight();
        QCOMPARE(m_document.cursor().position, (Position{0, 11}));
    }

    void wordLeftStopsAtWordStarts()
    {
        m_document.setText(QStringLiteral("alpha beta gamma"));
        m_document.setCursorPosition(Position{0, 16});

        m_document.moveWordLeft();
        QCOMPARE(m_document.cursor().position, (Position{0, 11}));

        m_document.moveWordLeft();
        QCOMPARE(m_document.cursor().position, (Position{0, 6}));
    }

    void underscoresAreWordCharacters()
    {
        // snake_case is one identifier to a programmer; stopping inside it
        // would be wrong in every language Keys is likely to open.
        m_document.setText(QStringLiteral("snake_case next"));
        m_document.setCursorPosition(Position{0, 0});

        m_document.moveWordRight();
        QCOMPARE(m_document.cursor().position, (Position{0, 11}));
    }

    void punctuationIsItsOwnRun()
    {
        m_document.setText(QStringLiteral("foo(bar)"));
        m_document.setCursorPosition(Position{0, 0});

        m_document.moveWordRight();
        QCOMPARE(m_document.cursor().position, (Position{0, 3}));  // before "("
    }

    void wordMovementCrossesLineBoundaries()
    {
        m_document.setText(QStringLiteral("one\ntwo"));
        m_document.setCursorPosition(Position{0, 3});
        m_document.moveWordRight();
        QCOMPARE(m_document.cursor().position, (Position{1, 0}));

        m_document.moveWordLeft();
        QCOMPARE(m_document.cursor().position, (Position{0, 3}));
    }

    // ---- Selection --------------------------------------------------------

    void extendingBuildsASelection()
    {
        m_document.setText(QStringLiteral("abcdef"));
        m_document.setCursorPosition(Position{0, 1});
        m_document.moveRight(true);
        m_document.moveRight(true);

        QVERIFY(m_document.cursor().hasSelection());
        QCOMPARE(m_document.selectedText(), QStringLiteral("bc"));
    }

    void movingWithoutExtendingCollapsesTheSelection()
    {
        m_document.setText(QStringLiteral("abcdef"));
        m_document.setCursorPosition(Position{0, 1});
        m_document.setCursorPosition(Position{0, 4}, true);

        // Left collapses to the selection's start, not one before it.
        m_document.moveLeft();
        QVERIFY(!m_document.cursor().hasSelection());
        QCOMPARE(m_document.cursor().position, (Position{0, 1}));
    }

    void selectAllCoversTheDocument()
    {
        m_document.setText(QStringLiteral("one\ntwo"));
        m_document.selectAll();
        QCOMPARE(m_document.selectedText(), QStringLiteral("one\ntwo"));
    }

    void selectLineIncludesItsTerminator()
    {
        // Deleting a selected line must remove it entirely rather than leaving
        // a blank line behind.
        m_document.setText(QStringLiteral("one\ntwo\nthree"));
        m_document.selectLine(1);
        m_document.deleteBackward();
        QCOMPARE(m_document.text(), QStringLiteral("one\nthree"));
    }

    void selectingTheLastLineHasNoTerminator()
    {
        m_document.setText(QStringLiteral("one\ntwo"));
        m_document.selectLine(1);
        QCOMPARE(m_document.selectedText(), QStringLiteral("two"));
    }

    void backwardSelectionYieldsTheSameText()
    {
        m_document.setText(QStringLiteral("abcdef"));
        m_document.setCursorPosition(Position{0, 4});
        m_document.setCursorPosition(Position{0, 1}, true);
        QCOMPARE(m_document.selectedText(), QStringLiteral("bcd"));
    }

    // ---- Signals ----------------------------------------------------------

    void editingEmitsContentsChanged()
    {
        QSignalSpy spy(&m_document, &TextDocument::contentsChanged);
        m_document.insertText(QStringLiteral("line\n"));
        QCOMPARE(spy.count(), 1);
        // The view uses the line delta to update incrementally.
        QCOMPARE(spy.at(0).at(1).toInt(), 1);
    }

    void movingEmitsCursorChanged()
    {
        m_document.setText(QStringLiteral("abc"));
        QSignalSpy spy(&m_document, &TextDocument::cursorChanged);
        m_document.moveRight();
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TextDocumentTests)
#include "TextDocumentTests.moc"
