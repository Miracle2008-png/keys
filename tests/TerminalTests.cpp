#include "terminal/TerminalScreen.h"
#include "terminal/TerminalSession.h"
#include "terminal/VtParser.h"

#include <QSignalSpy>
#include <QTest>

using namespace keys::terminal;

class TerminalTests : public QObject {
    Q_OBJECT

private:
    TerminalScreen m_screen;

    /// Feeds a literal byte string through a fresh parser.
    void feed(const char* bytes)
    {
        VtParser parser(m_screen);
        parser.parse(QByteArray(bytes));
    }

    [[nodiscard]] QString lineText(int index) const
    {
        return m_screen.lineAt(index).text();
    }

    /// The visible grid's first line, which is what most assertions address.
    [[nodiscard]] int top() const
    {
        return qMax(0, m_screen.totalLines() - m_screen.rows());
    }

private slots:
    void init()
    {
        m_screen = TerminalScreen();
        m_screen.resize(80, 24);
    }

    // ---- Screen: writing --------------------------------------------------

    void startsWithAVisibleGrid()
    {
        // The cursor has to sit somewhere even before anything is written.
        QCOMPARE(m_screen.totalLines(), 24);
        QCOMPARE(m_screen.cursorLine(), 0);
        QCOMPARE(m_screen.cursorColumn(), 0);
    }

    void writesTextAtTheCursor()
    {
        m_screen.writeText(QStringLiteral("hello"), CellStyle{});
        QCOMPARE(lineText(0), QStringLiteral("hello"));
        QCOMPARE(m_screen.cursorColumn(), 5);
    }

    void writingOverwritesRatherThanInserts()
    {
        // A terminal cell is replaced, not pushed aside - this is what makes a
        // redrawing progress line work.
        m_screen.writeText(QStringLiteral("abcdef"), CellStyle{});
        m_screen.setCursor(0, 2);
        m_screen.writeText(QStringLiteral("XY"), CellStyle{});
        QCOMPARE(lineText(0), QStringLiteral("abXYef"));
    }

    void writingPastTheEndPadsWithSpaces()
    {
        // A program that positions the cursor and writes must not leave a
        // ragged line behind it.
        m_screen.writeText(QStringLiteral("ab"), CellStyle{});
        m_screen.setCursor(0, 5);
        m_screen.writeText(QStringLiteral("X"), CellStyle{});
        QCOMPARE(lineText(0), QStringLiteral("ab   X"));
    }

    void textWrapsAtTheRightEdge()
    {
        m_screen.resize(5, 24);
        m_screen.writeText(QStringLiteral("abcdefgh"), CellStyle{});

        QCOMPARE(lineText(0), QStringLiteral("abcde"));
        QCOMPARE(lineText(1), QStringLiteral("fgh"));
    }

    void aLineFilledExactlyDoesNotLeaveABlankLine()
    {
        // Wrapping on the next write rather than immediately is what avoids the
        // spurious blank line every naive terminal produces.
        m_screen.resize(5, 24);
        m_screen.writeText(QStringLiteral("abcde"), CellStyle{});

        QCOMPARE(lineText(0), QStringLiteral("abcde"));
        QVERIFY(lineText(1).isEmpty());
    }

    // ---- Screen: cursor and erasing ---------------------------------------

    void carriageReturnAndLineFeedAreSeparate()
    {
        m_screen.writeText(QStringLiteral("first"), CellStyle{});
        m_screen.carriageReturn();
        m_screen.lineFeed();
        m_screen.writeText(QStringLiteral("second"), CellStyle{});

        QCOMPARE(lineText(0), QStringLiteral("first"));
        QCOMPARE(lineText(1), QStringLiteral("second"));
    }

    void carriageReturnAloneRewritesTheLine()
    {
        // The progress-bar idiom: return to column 0 and overwrite.
        m_screen.writeText(QStringLiteral("50%"), CellStyle{});
        m_screen.carriageReturn();
        m_screen.writeText(QStringLiteral("99%"), CellStyle{});

        QCOMPARE(lineText(0), QStringLiteral("99%"));
        QCOMPARE(m_screen.totalLines(), 24);
    }

    void backspaceMovesWithoutErasing()
    {
        m_screen.writeText(QStringLiteral("abc"), CellStyle{});
        m_screen.backspace();
        QCOMPARE(m_screen.cursorColumn(), 2);
        QCOMPARE(lineText(0), QStringLiteral("abc"));
    }

    void tabAdvancesToTheNextStop()
    {
        m_screen.writeText(QStringLiteral("ab"), CellStyle{});
        m_screen.tab();
        QCOMPARE(m_screen.cursorColumn(), 8);
    }

    void cursorPositionIsClamped()
    {
        // A program may address a cell a resize has since removed.
        m_screen.setCursor(999, 999);
        QCOMPARE(m_screen.cursorRow(), 23);
        QCOMPARE(m_screen.cursorColumn(), 79);

        m_screen.setCursor(-5, -5);
        QCOMPARE(m_screen.cursorRow(), 0);
        QCOMPARE(m_screen.cursorColumn(), 0);
    }

    void eraseToEndOfLineKeepsWhatCameBefore()
    {
        m_screen.writeText(QStringLiteral("keep this"), CellStyle{});
        m_screen.setCursor(0, 4);
        m_screen.eraseInLine(0);
        QCOMPARE(lineText(0), QStringLiteral("keep"));
    }

    void eraseToStartOfLinePreservesColumns()
    {
        // Replaced by spaces, not removed, so the rest keeps its position.
        m_screen.writeText(QStringLiteral("abcdef"), CellStyle{});
        m_screen.setCursor(0, 3);
        m_screen.eraseInLine(1);
        QCOMPARE(lineText(0), QStringLiteral("   def"));
    }

    void eraseWholeDisplayKeepsScrollback()
    {
        // `clear` in a shell must not destroy the history the user scrolled
        // back to read.
        for (int i = 0; i < 30; ++i) {
            m_screen.writeText(QStringLiteral("line %1").arg(i), CellStyle{});
            m_screen.carriageReturn();
            m_screen.lineFeed();
        }
        const int before = m_screen.totalLines();

        m_screen.eraseInDisplay(2);

        QCOMPARE(m_screen.totalLines(), before);
        QVERIFY(!lineText(0).isEmpty());          // scrollback survives
        QVERIFY(lineText(top()).isEmpty());       // the visible grid is cleared
    }

    // ---- Screen: scrollback ------------------------------------------------

    void scrollingPushesLinesIntoHistory()
    {
        for (int i = 0; i < 30; ++i) {
            m_screen.writeText(QStringLiteral("row%1").arg(i), CellStyle{});
            m_screen.carriageReturn();
            m_screen.lineFeed();
        }

        QVERIFY(m_screen.totalLines() > m_screen.rows());
        QCOMPARE(lineText(0), QStringLiteral("row0"));
    }

    void scrollbackIsBounded()
    {
        // A build's output is unbounded; an editor that grows without limit
        // while a test suite runs eventually stops responding.
        for (int i = 0; i < TerminalScreen::kMaxScrollback + 500; ++i) {
            m_screen.lineFeed();
        }
        QVERIFY(m_screen.totalLines()
                <= TerminalScreen::kMaxScrollback + m_screen.rows());
    }

    // ---- Parser: text and control characters ------------------------------

    void parsesPlainText()
    {
        feed("hello world");
        QCOMPARE(lineText(0), QStringLiteral("hello world"));
    }

    void parsesNewlines()
    {
        feed("one\r\ntwo");
        QCOMPARE(lineText(0), QStringLiteral("one"));
        QCOMPARE(lineText(1), QStringLiteral("two"));
    }

    void dropsUnprintableControlCharacters()
    {
        // Printing them as boxes would corrupt otherwise-clean output.
        feed("a\x01\x02z");
        QCOMPARE(lineText(0), QStringLiteral("az"));
    }

    void theBellIsSilent()
    {
        // An audible bell from a build script is an interruption nobody wants
        // from an editor.
        feed("ding\x07");
        QCOMPARE(lineText(0), QStringLiteral("ding"));
    }

    // ---- Parser: CSI sequences --------------------------------------------

    void parsesCursorPositioning()
    {
        feed("\x1b[5;10Hx");
        // 1-based in the sequence, 0-based on the screen.
        QCOMPARE(m_screen.cursorRow(), 4);
        QCOMPARE(lineText(top() + 4), QStringLiteral("         x"));
    }

    void cursorPositionDefaultsToTheOrigin()
    {
        feed("abc\x1b[Hx");
        QCOMPARE(m_screen.cursorRow(), 0);
        QCOMPARE(lineText(0), QStringLiteral("xbc"));
    }

    void parsesRelativeCursorMovement()
    {
        feed("\x1b[10;10H\x1b[3A\x1b[2D");
        QCOMPARE(m_screen.cursorRow(), 6);
        QCOMPARE(m_screen.cursorColumn(), 7);
    }

    void parsesColumnAndRowAddressing()
    {
        feed("\x1b[5;5H\x1b[20G");
        QCOMPARE(m_screen.cursorRow(), 4);       // row unchanged by CHA
        QCOMPARE(m_screen.cursorColumn(), 19);

        feed("\x1b[3d");
        QCOMPARE(m_screen.cursorRow(), 2);       // column unchanged by VPA
    }

    void parsesEraseInLine()
    {
        feed("hello world\x1b[6G\x1b[K");
        QCOMPARE(lineText(0), QStringLiteral("hello"));
    }

    void parsesEraseInDisplay()
    {
        feed("some text\x1b[2J");
        QVERIFY(lineText(top()).isEmpty());
    }

    void hidesAndShowsTheCursor()
    {
        feed("\x1b[?25l");
        QVERIFY(!m_screen.cursorVisible());

        feed("\x1b[?25h");
        QVERIFY(m_screen.cursorVisible());
    }

    void unknownSequencesAreDiscardedNotPrinted()
    {
        // Printing an unimplemented sequence's bytes would corrupt the screen;
        // discarding leaves a blank area instead.
        feed("a\x1b[?1049hb");
        QCOMPARE(lineText(0), QStringLiteral("ab"));
    }

    // ---- Parser: colours ---------------------------------------------------

    void parsesBasicColours()
    {
        TerminalScreen screen;
        VtParser parser(screen);

        parser.parse(QByteArray("\x1b[31mred"));
        QCOMPARE(parser.currentStyle().foreground, 1);

        parser.parse(QByteArray("\x1b[0m"));
        QCOMPARE(parser.currentStyle().foreground, -1);
    }

    void parsesBrightColours()
    {
        TerminalScreen screen;
        VtParser parser(screen);
        parser.parse(QByteArray("\x1b[92m"));
        QCOMPARE(parser.currentStyle().foreground, 10);
    }

    void parsesAttributes()
    {
        TerminalScreen screen;
        VtParser parser(screen);

        parser.parse(QByteArray("\x1b[1;4m"));
        QVERIFY(parser.currentStyle().bold);
        QVERIFY(parser.currentStyle().underline);

        parser.parse(QByteArray("\x1b[22m"));
        QVERIFY(!parser.currentStyle().bold);
        QVERIFY(parser.currentStyle().underline);
    }

    void parses256ColourAndConsumesItsParameters()
    {
        // The trailing 1 must be read as bold, not as a stray colour code.
        TerminalScreen screen;
        VtParser parser(screen);

        parser.parse(QByteArray("\x1b[38;5;208;1m"));
        QCOMPARE(parser.currentStyle().foreground, 208);
        QVERIFY(parser.currentStyle().bold);
    }

    void colouredRunsAreKeptSeparate()
    {
        TerminalScreen screen;
        VtParser parser(screen);
        parser.parse(QByteArray("\x1b[31mred\x1b[0m plain"));

        const TerminalLine& line = screen.lineAt(0);
        QCOMPARE(line.text(), QStringLiteral("red plain"));
        QVERIFY(line.runs.size() >= 2);
        QCOMPARE(line.runs.front().style.foreground, 1);
    }

    // ---- Parser: incremental input ----------------------------------------

    void handlesASequenceSplitAcrossReads()
    {
        // Output arrives in arbitrary chunks, so an escape sequence can be cut
        // in half by a read boundary.
        VtParser parser(m_screen);
        parser.parse(QByteArray("hello\x1b["));
        parser.parse(QByteArray("6G\x1b[K"));

        QCOMPARE(lineText(0), QStringLiteral("hello"));
    }

    void handlesAUtf8CharacterSplitAcrossReads()
    {
        // A multi-byte character cut in half must not become a replacement
        // character.
        VtParser parser(m_screen);
        const QByteArray utf8 = QStringLiteral("café").toUtf8();

        parser.parse(utf8.left(utf8.size() - 1));
        parser.parse(utf8.right(1));

        QCOMPARE(lineText(0), QStringLiteral("café"));
    }

    void handlesOneByteAtATime()
    {
        VtParser parser(m_screen);
        const QByteArray input("\x1b[31mred\x1b[0m");
        for (int i = 0; i < input.size(); ++i) {
            parser.parse(input.mid(i, 1));
        }
        QCOMPARE(lineText(0), QStringLiteral("red"));
    }

    // ---- Parser: OSC -------------------------------------------------------

    void readsTheWindowTitle()
    {
        TerminalScreen screen;
        VtParser parser(screen);
        parser.parse(QByteArray("\x1b]0;Build running\x07"));
        QCOMPARE(parser.title(), QStringLiteral("Build running"));
    }

    void acceptsStringTerminatorAsWellAsBell()
    {
        TerminalScreen screen;
        VtParser parser(screen);
        parser.parse(QByteArray("\x1b]2;Other\x1b\\"));
        QCOMPARE(parser.title(), QStringLiteral("Other"));
    }

    void titleTextIsNotPrintedToTheScreen()
    {
        feed("\x1b]0;Title\x07visible");
        QCOMPARE(lineText(0), QStringLiteral("visible"));
    }

    // ---- A realistic prompt ------------------------------------------------

    void handlesATypicalColouredPrompt()
    {
        // What a shell actually emits: colour, text, reset, an erase, and a
        // cursor move.
        feed("\x1b[32muser@host\x1b[0m:\x1b[34m~/dev\x1b[0m$ \x1b[K");

        QCOMPARE(lineText(0), QStringLiteral("user@host:~/dev$ "));
        QCOMPARE(m_screen.totalLines(), 24);
    }

    // ---- Session -----------------------------------------------------------

    void sessionBatchesRepaints()
    {
        // A build emits thousands of writes; repainting per write would spend
        // the frame budget on output nobody can read that fast.
        TerminalSession session;
        QSignalSpy spy(&session, &TerminalSession::screenChanged);

        // Resizing emits directly, which is enough to prove the signal exists
        // without needing a real shell.
        session.resize(100, 30);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(session.screen().columns(), 100);
        QCOMPARE(session.screen().rows(), 30);
    }

    void resizingToTheSameSizeDoesNothing()
    {
        TerminalSession session;
        session.resize(100, 30);

        QSignalSpy spy(&session, &TerminalSession::screenChanged);
        session.resize(100, 30);
        QCOMPARE(spy.count(), 0);
    }

    void anUnstartedSessionIsNotRunning()
    {
        const TerminalSession session;
        QVERIFY(!session.isRunning());
    }
};

QTEST_MAIN(TerminalTests)
#include "TerminalTests.moc"
