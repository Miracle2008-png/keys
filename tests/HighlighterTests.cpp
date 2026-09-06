#include "editor/SyntaxHighlighter.h"

#include <QTest>

using namespace keys::editor;

/// Lexical highlighting.
///
/// The cases that matter are the ones where a naive tokeniser goes wrong and
/// keeps going wrong for the rest of the file: an escaped quote inside a string,
/// a block comment that spans lines, a keyword that is only part of a longer
/// identifier.
class HighlighterTests : public QObject {
    Q_OBJECT

private:
    /// The kind covering `needle`'s first occurrence, or Plain when nothing
    /// covers it.
    [[nodiscard]] static TokenKind kindOf(const QString& line, const QString& needle,
                                          SyntaxHighlighter::Language language,
                                          LineState incoming = LineState::Normal)
    {
        const SyntaxHighlighter highlighter(language);
        LineState outgoing = LineState::Normal;
        const std::vector<Token> tokens = highlighter.tokenize(line, incoming, outgoing);

        const int at = static_cast<int>(line.indexOf(needle));
        if (at < 0) {
            return TokenKind::Plain;
        }

        for (const Token& token : tokens) {
            if (at >= token.start && at < token.start + token.length) {
                return token.kind;
            }
        }
        return TokenKind::Plain;
    }

private slots:
    // ---- Language detection ------------------------------------------------

    void detectsLanguagesFromExtensions()
    {
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.cpp")),
                 SyntaxHighlighter::Language::C);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.h")),
                 SyntaxHighlighter::Language::C);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.PY")),
                 SyntaxHighlighter::Language::Python);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.rs")),
                 SyntaxHighlighter::Language::Rust);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.qml")),
                 SyntaxHighlighter::Language::Qml);
    }

    void leavesUnknownFileTypesAlone()
    {
        // Mis-colouring reads as a bug; no colour reads as a file type Keys does
        // not know yet.
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("notes.xyz")),
                 SyntaxHighlighter::Language::None);

        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::None);
        LineState outgoing = LineState::Normal;
        QVERIFY(highlighter
                    .tokenize(QStringLiteral("int x = 1;"), LineState::Normal, outgoing)
                    .empty());
    }

    // ---- Basic tokens ------------------------------------------------------

    void coloursKeywords()
    {
        QCOMPARE(kindOf(QStringLiteral("return value;"), QStringLiteral("return"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Keyword);
    }

    void coloursTypesSeparatelyFromKeywords()
    {
        // The design's palette distinguishes them, and it makes a declaration
        // readable at a glance.
        QCOMPARE(kindOf(QStringLiteral("const int value = 1;"), QStringLiteral("int"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Type);
        QCOMPARE(kindOf(QStringLiteral("const int value = 1;"), QStringLiteral("const"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Keyword);
    }

    void coloursNumbersIncludingHexAndFloats()
    {
        for (const QString& literal : {QStringLiteral("42"), QStringLiteral("0xFF"),
                                       QStringLiteral("1.5e3"), QStringLiteral("1'000")}) {
            const QString line = QStringLiteral("x = %1;").arg(literal);
            QCOMPARE(kindOf(line, literal, SyntaxHighlighter::Language::C),
                     TokenKind::Number);
        }
    }

    void coloursACallAsAFunction()
    {
        QCOMPARE(kindOf(QStringLiteral("compute(a, b);"), QStringLiteral("compute"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Function);

        // A bare identifier is not a call, and colouring it as one would make
        // every variable look like a function.
        QCOMPARE(kindOf(QStringLiteral("value + 1;"), QStringLiteral("value"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Plain);
    }

    void doesNotColourAKeywordInsideALongerIdentifier()
    {
        // "returnValue" is not "return". A substring match would colour half of
        // every identifier in the file.
        QCOMPARE(kindOf(QStringLiteral("int returnValue = 1;"),
                        QStringLiteral("returnValue"), SyntaxHighlighter::Language::C),
                 TokenKind::Plain);

        QCOMPARE(kindOf(QStringLiteral("int intensity = 1;"), QStringLiteral("intensity"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Plain);
    }

    // ---- Strings -----------------------------------------------------------

    void coloursStrings()
    {
        QCOMPARE(kindOf(QStringLiteral("s = \"hello\";"), QStringLiteral("hello"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::String);
    }

    void handlesAnEscapedQuoteInsideAString()
    {
        // The case that ruins the rest of a line: stopping at the escaped quote
        // would leave `b" + trailing` coloured as code and the real close quote
        // opening a new string.
        const QString line = QStringLiteral("s = \"a\\\"b\"; int after = 1;");

        QCOMPARE(kindOf(line, QStringLiteral("b"), SyntaxHighlighter::Language::C),
                 TokenKind::String);
        QCOMPARE(kindOf(line, QStringLiteral("int"), SyntaxHighlighter::Language::C),
                 TokenKind::Type);
    }

    void anUnterminatedStringDoesNotSwallowTheNextLine()
    {
        // It runs to the end of its own line and stops there. Carrying it
        // forward would colour the whole file after one stray quote.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::C);
        LineState outgoing = LineState::Normal;
        (void)highlighter.tokenize(QStringLiteral("s = \"unterminated"),
                                   LineState::Normal, outgoing);
        QCOMPARE(outgoing, LineState::Normal);
    }

    // ---- Comments ----------------------------------------------------------

    void coloursLineComments()
    {
        QCOMPARE(kindOf(QStringLiteral("int x = 1; // why"), QStringLiteral("why"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Comment);

        QCOMPARE(kindOf(QStringLiteral("x = 1  # why"), QStringLiteral("why"),
                        SyntaxHighlighter::Language::Python),
                 TokenKind::Comment);
    }

    void aBlockCommentCarriesToTheNextLine()
    {
        // The reason lines carry state at all.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::C);

        LineState after = LineState::Normal;
        (void)highlighter.tokenize(QStringLiteral("/* opening"), LineState::Normal, after);
        QCOMPARE(after, LineState::InBlockComment);

        // The next line is entirely comment, even though it looks like code.
        QCOMPARE(kindOf(QStringLiteral("int notReallyCode = 1;"), QStringLiteral("int"),
                        SyntaxHighlighter::Language::C, LineState::InBlockComment),
                 TokenKind::Comment);
    }

    void aBlockCommentClosesAndCodeResumes()
    {
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::C);

        LineState after = LineState::InBlockComment;
        LineState outgoing = LineState::Normal;
        (void)highlighter.tokenize(QStringLiteral("closing */ int x = 1;"),
                                   after, outgoing);
        QCOMPARE(outgoing, LineState::Normal);

        QCOMPARE(kindOf(QStringLiteral("closing */ int x = 1;"), QStringLiteral("int"),
                        SyntaxHighlighter::Language::C, LineState::InBlockComment),
                 TokenKind::Type);
    }

    void aBlockCommentOpenedAndClosedOnOneLineDoesNotCarry()
    {
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::C);
        LineState outgoing = LineState::InBlockComment;
        (void)highlighter.tokenize(QStringLiteral("int x /* note */ = 1;"),
                                   LineState::Normal, outgoing);
        QCOMPARE(outgoing, LineState::Normal);
    }

    void languagesWithoutBlockCommentsDoNotCarryState()
    {
        // Python has no /* */, so a line containing one must not put the rest of
        // the file in a comment.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::Python);
        LineState outgoing = LineState::Normal;
        (void)highlighter.tokenize(QStringLiteral("x = a /* not a comment"),
                                   LineState::Normal, outgoing);
        QCOMPARE(outgoing, LineState::Normal);
    }

    // ---- Per-language ------------------------------------------------------

    void coloursPythonKeywords()
    {
        QCOMPARE(kindOf(QStringLiteral("def compute(x):"), QStringLiteral("def"),
                        SyntaxHighlighter::Language::Python),
                 TokenKind::Keyword);
    }

    void coloursRustKeywordsAndTypes()
    {
        QCOMPARE(kindOf(QStringLiteral("fn value() -> u32 {"), QStringLiteral("fn"),
                        SyntaxHighlighter::Language::Rust),
                 TokenKind::Keyword);
        QCOMPARE(kindOf(QStringLiteral("fn value() -> u32 {"), QStringLiteral("u32"),
                        SyntaxHighlighter::Language::Rust),
                 TokenKind::Type);
    }

    void coloursMarkdownHeadings()
    {
        QCOMPARE(kindOf(QStringLiteral("## A heading"), QStringLiteral("heading"),
                        SyntaxHighlighter::Language::Markdown),
                 TokenKind::Keyword);
    }

    // ---- Robustness --------------------------------------------------------

    void tokensNeverOverlapOrExceedTheLine()
    {
        // A malformed token list would make the rich-text builder produce
        // garbage or read past the end of the string.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::C);

        for (const QString& line : {
                 QStringLiteral("int x = \"a\" + 1; // c"),
                 QStringLiteral("/* a */ b /* c"),
                 QStringLiteral("\"\"\"\"\"\""),
                 QStringLiteral("x=1;y=2;/*"),
                 QStringLiteral("    "),
             }) {
            LineState outgoing = LineState::Normal;
            const std::vector<Token> tokens =
                highlighter.tokenize(line, LineState::Normal, outgoing);

            int previousEnd = 0;
            for (const Token& token : tokens) {
                QVERIFY2(token.start >= previousEnd, qPrintable(line));
                QVERIFY2(token.length > 0, qPrintable(line));
                QVERIFY2(token.start + token.length <= line.size(), qPrintable(line));
                previousEnd = token.start + token.length;
            }
        }
    }

    void anEmptyLineProducesNoTokens()
    {
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::C);
        LineState outgoing = LineState::Normal;
        QVERIFY(highlighter.tokenize(QString(), LineState::Normal, outgoing).empty());
    }
};

QTEST_MAIN(HighlighterTests)
#include "HighlighterTests.moc"
