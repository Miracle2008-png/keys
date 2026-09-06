#include "editor/TextDocument.h"

#include <QTest>

using namespace keys::editor;

/// Editing at more than one caret.
///
/// The case that matters is ordering. An edit at an earlier position shifts
/// everything after it, so applying carets in document order leaves every
/// later one pointing at the wrong character - and the damage is silent, which
/// is the worst kind. These tests pin the order and the undo grouping.
class MultiCursorTests : public QObject {
    Q_OBJECT

private:
    TextDocument m_document;

    void load(const QString& text) { m_document.setText(text); }

private slots:
    void init() { m_document.setText(QString()); }

    // ---- Managing carets ---------------------------------------------------

    void startsWithExactlyOneCaret()
    {
        load(QStringLiteral("one\ntwo"));
        QCOMPARE(m_document.cursorCount(), 1);
        QVERIFY(!m_document.hasMultipleCursors());
    }

    void addsACaret()
    {
        load(QStringLiteral("one\ntwo"));
        m_document.addCursor(Position{1, 0});

        QCOMPARE(m_document.cursorCount(), 2);
        QVERIFY(m_document.hasMultipleCursors());
    }

    void refusesADuplicateCaret()
    {
        // Two carets in one place would type every character twice.
        load(QStringLiteral("one\ntwo"));
        m_document.setCursorPosition(Position{0, 0});

        m_document.addCursor(Position{0, 0});
        QCOMPARE(m_document.cursorCount(), 1);

        m_document.addCursor(Position{1, 0});
        m_document.addCursor(Position{1, 0});
        QCOMPARE(m_document.cursorCount(), 2);
    }

    void clearingLeavesThePrimary()
    {
        load(QStringLiteral("one\ntwo\nthree"));
        m_document.addCursor(Position{1, 0});
        m_document.addCursor(Position{2, 0});
        QCOMPARE(m_document.cursorCount(), 3);

        m_document.clearExtraCursors();
        QCOMPARE(m_document.cursorCount(), 1);
    }

    void cursorsAreReportedInDocumentOrder()
    {
        load(QStringLiteral("a\nb\nc"));
        m_document.setCursorPosition(Position{2, 0});
        m_document.addCursor(Position{0, 0});
        m_document.addCursor(Position{1, 0});

        const std::vector<Cursor> all = m_document.cursors();
        QCOMPARE(static_cast<int>(all.size()), 3);
        QCOMPARE(all[0].position.line, 0);
        QCOMPARE(all[1].position.line, 1);
        QCOMPARE(all[2].position.line, 2);
    }

    void addsBelowFromTheLowestCaret()
    {
        // Repeating the shortcut has to walk down the file rather than adding
        // the same caret again.
        load(QStringLiteral("a\nb\nc\nd"));
        m_document.setCursorPosition(Position{0, 0});

        m_document.addCursorBelow();
        m_document.addCursorBelow();

        const std::vector<Cursor> all = m_document.cursors();
        QCOMPARE(static_cast<int>(all.size()), 3);
        QCOMPARE(all[2].position.line, 2);
    }

    void addingBelowStopsAtTheLastLine()
    {
        load(QStringLiteral("only"));
        m_document.addCursorBelow();
        QCOMPARE(m_document.cursorCount(), 1);
    }

    void addingAboveStopsAtTheFirstLine()
    {
        load(QStringLiteral("only"));
        m_document.setCursorPosition(Position{0, 0});
        m_document.addCursorAbove();
        QCOMPARE(m_document.cursorCount(), 1);
    }

    // ---- Editing -----------------------------------------------------------

    void typesAtEveryCaret()
    {
        load(QStringLiteral("a\nb\nc"));
        m_document.setCursorPosition(Position{0, 1});
        m_document.addCursor(Position{1, 1});
        m_document.addCursor(Position{2, 1});

        m_document.insertText(QStringLiteral("!"));

        QCOMPARE(m_document.text(), QStringLiteral("a!\nb!\nc!"));
    }

    void typingKeepsEveryCaret()
    {
        load(QStringLiteral("a\nb"));
        m_document.setCursorPosition(Position{0, 1});
        m_document.addCursor(Position{1, 1});

        m_document.insertText(QStringLiteral("x"));

        QCOMPARE(m_document.cursorCount(), 2);
    }

    void editsOnOneLineDoNotCorruptEachOther()
    {
        // Two carets on the same line is where document-order application goes
        // wrong: the second lands one character off for every character the
        // first inserted.
        load(QStringLiteral("abcd"));
        m_document.setCursorPosition(Position{0, 1});
        m_document.addCursor(Position{0, 3});

        m_document.insertText(QStringLiteral("-"));

        QCOMPARE(m_document.text(), QStringLiteral("a-bc-d"));
    }

    void aLongerInsertionStillLandsCorrectly()
    {
        load(QStringLiteral("abcd"));
        m_document.setCursorPosition(Position{0, 1});
        m_document.addCursor(Position{0, 3});

        m_document.insertText(QStringLiteral("XYZ"));

        QCOMPARE(m_document.text(), QStringLiteral("aXYZbcXYZd"));
    }

    void backspaceAtEveryCaret()
    {
        load(QStringLiteral("ax\nbx\ncx"));
        m_document.setCursorPosition(Position{0, 2});
        m_document.addCursor(Position{1, 2});
        m_document.addCursor(Position{2, 2});

        m_document.deleteBackward();

        QCOMPARE(m_document.text(), QStringLiteral("a\nb\nc"));
    }

    void deleteForwardAtEveryCaret()
    {
        load(QStringLiteral("xa\nxb"));
        m_document.setCursorPosition(Position{0, 0});
        m_document.addCursor(Position{1, 0});

        m_document.deleteForward();

        QCOMPARE(m_document.text(), QStringLiteral("a\nb"));
    }

    void backspaceAtTheStartOfTheDocumentIsIgnoredForThatCaret()
    {
        // One caret having nothing to delete must not stop the others.
        load(QStringLiteral("ab\ncd"));
        m_document.setCursorPosition(Position{0, 0});
        m_document.addCursor(Position{1, 1});

        m_document.deleteBackward();

        QCOMPARE(m_document.text(), QStringLiteral("ab\nd"));
    }

    // ---- Undo --------------------------------------------------------------

    void oneUndoTakesBackEveryCaretsEdit()
    {
        // Typing a character at four carets is one action as far as the user is
        // concerned; four presses of Ctrl+Z to take it back would be absurd.
        load(QStringLiteral("a\nb\nc"));
        m_document.setCursorPosition(Position{0, 1});
        m_document.addCursor(Position{1, 1});
        m_document.addCursor(Position{2, 1});

        m_document.insertText(QStringLiteral("!"));
        QCOMPARE(m_document.text(), QStringLiteral("a!\nb!\nc!"));

        QVERIFY(m_document.undo());
        QCOMPARE(m_document.text(), QStringLiteral("a\nb\nc"));
    }

    // ---- Interaction with the rest of the editor ---------------------------

    void loadingADocumentDropsExtraCarets()
    {
        load(QStringLiteral("a\nb"));
        m_document.addCursor(Position{1, 0});
        QCOMPARE(m_document.cursorCount(), 2);

        load(QStringLiteral("fresh"));
        QCOMPARE(m_document.cursorCount(), 1);
    }

    void aSingleCaretStillBehavesExactlyAsBefore()
    {
        // The whole design rests on the ordinary path being untouched.
        load(QStringLiteral("hello"));
        m_document.setCursorPosition(Position{0, 5});
        m_document.insertText(QStringLiteral(" world"));

        QCOMPARE(m_document.text(), QStringLiteral("hello world"));
        QCOMPARE(m_document.cursorCount(), 1);
    }
};

QTEST_MAIN(MultiCursorTests)
#include "MultiCursorTests.moc"
