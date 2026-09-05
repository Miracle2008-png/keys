#include "editor/TextBuffer.h"

#include <QTest>

using namespace keys::editor;

class TextBufferTests : public QObject {
    Q_OBJECT

private slots:
    // ---- Construction -----------------------------------------------------

    void emptyBufferHasOneEmptyLine()
    {
        // An editor with zero lines has nowhere to put the caret, so an empty
        // document is one empty line.
        const TextBuffer buffer;
        QCOMPARE(buffer.lineCount(), 1);
        QCOMPARE(buffer.length(), 0);
        QVERIFY(buffer.isEmpty());
        QCOMPARE(buffer.line(0), QString());
    }

    void loadsInitialText()
    {
        const TextBuffer buffer(QStringLiteral("alpha\nbeta\ngamma"));
        QCOMPARE(buffer.lineCount(), 3);
        QCOMPARE(buffer.line(0), QStringLiteral("alpha"));
        QCOMPARE(buffer.line(1), QStringLiteral("beta"));
        QCOMPARE(buffer.line(2), QStringLiteral("gamma"));
    }

    void trailingNewlineMakesAFinalEmptyLine()
    {
        // A file ending in a newline has an empty last line, which is where the
        // caret goes when the user presses End on it. Collapsing it would make
        // the editor unable to represent the file's real end.
        const TextBuffer buffer(QStringLiteral("one\ntwo\n"));
        QCOMPARE(buffer.lineCount(), 3);
        QCOMPARE(buffer.line(2), QString());
    }

    void textRoundTrips()
    {
        const QString original = QStringLiteral("first\nsecond\n\nfourth");
        const TextBuffer buffer(original);
        QCOMPARE(buffer.text(), original);
    }

    // ---- Insertion --------------------------------------------------------

    void insertIntoEmptyBuffer()
    {
        TextBuffer buffer;
        const Position after = buffer.insert(Position{0, 0}, QStringLiteral("hello"));

        QCOMPARE(buffer.text(), QStringLiteral("hello"));
        QCOMPARE(after, (Position{0, 5}));
    }

    void insertInTheMiddleSplitsAPiece()
    {
        TextBuffer buffer(QStringLiteral("aaadddd"));
        buffer.insert(Position{0, 3}, QStringLiteral("bbbccc"));
        QCOMPARE(buffer.text(), QStringLiteral("aaabbbcccdddd"));
    }

    void insertAtLineStartAndEnd()
    {
        TextBuffer buffer(QStringLiteral("beta"));
        buffer.insert(Position{0, 0}, QStringLiteral("alpha "));
        QCOMPARE(buffer.text(), QStringLiteral("alpha beta"));

        buffer.insert(buffer.endPosition(), QStringLiteral(" gamma"));
        QCOMPARE(buffer.text(), QStringLiteral("alpha beta gamma"));
    }

    void insertingANewlineSplitsTheLine()
    {
        TextBuffer buffer(QStringLiteral("onetwo"));
        const Position after = buffer.insert(Position{0, 3}, QStringLiteral("\n"));

        QCOMPARE(buffer.lineCount(), 2);
        QCOMPARE(buffer.line(0), QStringLiteral("one"));
        QCOMPARE(buffer.line(1), QStringLiteral("two"));
        QCOMPARE(after, (Position{1, 0}));
    }

    void insertingMultipleLinesReportsTheRightCaret()
    {
        TextBuffer buffer(QStringLiteral("start end"));
        const Position after =
            buffer.insert(Position{0, 6}, QStringLiteral("one\ntwo\nthree"));

        QCOMPARE(buffer.lineCount(), 3);
        QCOMPARE(buffer.line(0), QStringLiteral("start one"));
        QCOMPARE(buffer.line(1), QStringLiteral("two"));
        QCOMPARE(buffer.line(2), QStringLiteral("threeend"));
        QCOMPARE(after, (Position{2, 5}));
    }

    void insertingEmptyTextChangesNothing()
    {
        TextBuffer buffer(QStringLiteral("unchanged"));
        buffer.insert(Position{0, 4}, QString());
        QCOMPARE(buffer.text(), QStringLiteral("unchanged"));
    }

    void manySequentialInsertsStayCorrect()
    {
        // Typing is the common case: many small inserts at a moving caret. This
        // is where a piece table earns its keep, and where an off-by-one in the
        // split logic would show up.
        TextBuffer buffer;
        QString expected;
        Position caret{0, 0};

        for (int i = 0; i < 200; ++i) {
            const QString fragment = QStringLiteral("x%1 ").arg(i);
            caret = buffer.insert(caret, fragment);
            expected += fragment;
        }

        QCOMPARE(buffer.text(), expected);
        QCOMPARE(buffer.length(), expected.size());
    }

    void insertsAtTheFrontRepeatedly()
    {
        // The opposite pattern: every insert splits the first piece.
        TextBuffer buffer(QStringLiteral("end"));
        for (int i = 0; i < 50; ++i) {
            buffer.insert(Position{0, 0}, QStringLiteral("a"));
        }
        QCOMPARE(buffer.text(), QString(50, QLatin1Char('a')) + QStringLiteral("end"));
    }

    // ---- Removal ----------------------------------------------------------

    void removeWithinOneLine()
    {
        TextBuffer buffer(QStringLiteral("abcdef"));
        const Position after = buffer.remove(Range{{0, 2}, {0, 4}});

        QCOMPARE(buffer.text(), QStringLiteral("abef"));
        QCOMPARE(after, (Position{0, 2}));
    }

    void removeAcrossLinesJoinsThem()
    {
        TextBuffer buffer(QStringLiteral("one\ntwo\nthree"));
        buffer.remove(Range{{0, 2}, {2, 2}});

        QCOMPARE(buffer.lineCount(), 1);
        QCOMPARE(buffer.line(0), QStringLiteral("onree"));
    }

    void removingANewlineJoinsTwoLines()
    {
        TextBuffer buffer(QStringLiteral("one\ntwo"));
        buffer.remove(Range{{0, 3}, {1, 0}});

        QCOMPARE(buffer.lineCount(), 1);
        QCOMPARE(buffer.line(0), QStringLiteral("onetwo"));
    }

    void removeEverything()
    {
        TextBuffer buffer(QStringLiteral("all\nof\nit"));
        buffer.remove(Range{{0, 0}, buffer.endPosition()});

        QCOMPARE(buffer.length(), 0);
        QCOMPARE(buffer.lineCount(), 1);
        QVERIFY(buffer.isEmpty());
    }

    void removingAnEmptyRangeChangesNothing()
    {
        TextBuffer buffer(QStringLiteral("intact"));
        buffer.remove(Range{{0, 3}, {0, 3}});
        QCOMPARE(buffer.text(), QStringLiteral("intact"));
    }

    void reversedRangeIsNormalised()
    {
        // A selection dragged upwards has its anchor after its cursor; removing
        // it must delete the same text as the forward selection.
        TextBuffer buffer(QStringLiteral("abcdef"));
        buffer.remove(Range{{0, 4}, {0, 2}});
        QCOMPARE(buffer.text(), QStringLiteral("abef"));
    }

    void removeSpanningSeveralPieces()
    {
        // Removal has to trim partial pieces at both ends and drop the ones
        // between - the case most likely to be got wrong.
        TextBuffer buffer(QStringLiteral("AAAA"));
        buffer.insert(Position{0, 2}, QStringLiteral("BBBB"));
        buffer.insert(Position{0, 4}, QStringLiteral("CCCC"));
        QCOMPARE(buffer.text(), QStringLiteral("AABBCCCCBBAA"));

        buffer.remove(Range{{0, 1}, {0, 11}});
        QCOMPARE(buffer.text(), QStringLiteral("AA"));
    }

    void insertAfterRemoveStaysConsistent()
    {
        TextBuffer buffer(QStringLiteral("hello world"));
        buffer.remove(Range{{0, 5}, {0, 11}});
        QCOMPARE(buffer.text(), QStringLiteral("hello"));

        buffer.insert(buffer.endPosition(), QStringLiteral(" again"));
        QCOMPARE(buffer.text(), QStringLiteral("hello again"));
    }

    // ---- Positions and offsets --------------------------------------------

    void positionAndOffsetAreInverses()
    {
        const TextBuffer buffer(QStringLiteral("one\ntwo\nthree"));

        for (int offset = 0; offset <= buffer.length(); ++offset) {
            const Position position = buffer.positionOf(offset);
            QCOMPARE(buffer.offsetOf(position), offset);
        }
    }

    void offsetsSkipTheNewline()
    {
        const TextBuffer buffer(QStringLiteral("ab\ncd"));
        QCOMPARE(buffer.offsetOf(Position{0, 2}), 2);  // end of line 0
        QCOMPARE(buffer.offsetOf(Position{1, 0}), 3);  // start of line 1
    }

    void outOfRangePositionsAreClamped()
    {
        // Callers routinely compute positions that a concurrent edit has
        // invalidated; clamping keeps that from being undefined behaviour.
        const TextBuffer buffer(QStringLiteral("ab\ncd"));

        QCOMPARE(buffer.clamp(Position{99, 99}), buffer.endPosition());
        QCOMPARE(buffer.clamp(Position{-5, -5}), (Position{0, 0}));
        QCOMPARE(buffer.clamp(Position{0, 99}), (Position{0, 2}));
    }

    void outOfRangeOffsetsAreClamped()
    {
        const TextBuffer buffer(QStringLiteral("abc"));
        QCOMPARE(buffer.positionOf(-10), (Position{0, 0}));
        QCOMPARE(buffer.positionOf(1000), (Position{0, 3}));
    }

    void lineAccessorsToleratedOutOfRange()
    {
        const TextBuffer buffer(QStringLiteral("only"));
        QCOMPARE(buffer.line(-1), QString());
        QCOMPARE(buffer.line(5), QString());
        QCOMPARE(buffer.lineLength(5), 0);
    }

    // ---- Extracting text --------------------------------------------------

    void textInRangeWithinALine()
    {
        const TextBuffer buffer(QStringLiteral("abcdef"));
        QCOMPARE(buffer.textIn(Range{{0, 1}, {0, 4}}), QStringLiteral("bcd"));
    }

    void textInRangeAcrossLines()
    {
        const TextBuffer buffer(QStringLiteral("one\ntwo\nthree"));
        QCOMPARE(buffer.textIn(Range{{0, 1}, {2, 2}}), QStringLiteral("ne\ntwo\nth"));
    }

    void textInEmptyRangeIsEmpty()
    {
        const TextBuffer buffer(QStringLiteral("abc"));
        QVERIFY(buffer.textIn(Range{{0, 1}, {0, 1}}).isEmpty());
    }

    // ---- Unicode ----------------------------------------------------------

    void handlesNonAsciiText()
    {
        // Source files are full of non-ASCII; the buffer must not corrupt it.
        const QString text = QStringLiteral("café\n日本語\nnaïve");
        const TextBuffer buffer(text);

        QCOMPARE(buffer.text(), text);
        QCOMPARE(buffer.lineCount(), 3);
        QCOMPARE(buffer.line(1), QStringLiteral("日本語"));
    }

    void surrogatePairsCountAsTwoCodeUnits()
    {
        // Columns are UTF-16 code units, matching Qt and LSP's default encoding.
        // An emoji outside the BMP therefore occupies two columns - documenting
        // it here so a future change to grapheme columns is a deliberate one.
        const QString emoji = QStringLiteral("\U0001F600");
        const TextBuffer buffer(emoji);

        QCOMPARE(buffer.lineLength(0), 2);
        QCOMPARE(buffer.text(), emoji);
    }

    // ---- Line endings -----------------------------------------------------

    void carriageReturnsAreRetainedNotSilentlyDropped()
    {
        // A CRLF file must round-trip byte for byte: rewriting line endings
        // behind the user's back would show up as a whole-file diff in git.
        const QString crlf = QStringLiteral("one\r\ntwo\r\n");
        const TextBuffer buffer(crlf);

        QCOMPARE(buffer.text(), crlf);
        QCOMPARE(buffer.lineCount(), 3);
        // The CR stays at the end of the line's content.
        QCOMPARE(buffer.line(0), QStringLiteral("one\r"));
    }
};

QTEST_MAIN(TextBufferTests)
#include "TextBufferTests.moc"
