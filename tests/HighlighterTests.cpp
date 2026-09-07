#include "editor/SyntaxHighlighter.h"

#include <QTest>

#include <tuple>

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

    void marksMarkdownInlineSpans()
    {
        // The gap this closes: only headings, fences and bullets were marked,
        // so a document - which is mostly prose with emphasis and links in it -
        // came out almost entirely plain. Measured over a real file, coverage
        // was 47%.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::Markdown);
        LineState outgoing = LineState::Normal;

        const QString line =
            QStringLiteral("Some **bold**, *italic*, `code`, and [a link](http://x.y).");
        const std::vector<Token> tokens =
            highlighter.tokenize(line, LineState::Normal, outgoing);

        QHash<TokenKind, int> found;
        for (const Token& token : tokens) {
            ++found[token.kind];
        }

        QVERIFY2(found.value(TokenKind::Keyword) >= 1, "bold was not marked");
        QVERIFY2(found.value(TokenKind::Type) >= 1, "italic was not marked");
        QVERIFY2(found.value(TokenKind::String) >= 1, "a code span was not marked");
        QVERIFY2(found.value(TokenKind::Constant) >= 1, "a link target was not marked");
    }

    void marksMarkdownOrderedListMarkers()
    {
        QCOMPARE(kindOf(QStringLiteral("1. First item"), QStringLiteral("1."),
                        SyntaxHighlighter::Language::Markdown),
                 TokenKind::Number);
    }

    void anUnclosedMarkdownMarkerDoesNotSwallowTheLine()
    {
        // A lone asterisk is ordinary in prose. Treating it as the start of
        // emphasis that never closes would colour the rest of the line, and
        // with a carried state, the rest of the document.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::Markdown);
        LineState outgoing = LineState::Normal;

        const QString line = QStringLiteral("A lone * asterisk and a ` backtick.");
        const std::vector<Token> tokens =
            highlighter.tokenize(line, LineState::Normal, outgoing);

        for (const Token& token : tokens) {
            QVERIFY(token.start >= 0);
            QVERIFY(token.start + token.length <= line.size());
        }
        QCOMPARE(outgoing, LineState::Normal);
    }

    void markdownFencesOwnTheirLine()
    {
        QCOMPARE(kindOf(QStringLiteral("```cpp"), QStringLiteral("```"),
                        SyntaxHighlighter::Language::Markdown),
                 TokenKind::Comment);
    }

    // ---- The languages added after the first seventeen ---------------------

    void recognisesTheExtensionsItClaims()
    {
        // A language with rules that languageForPath cannot reach is a language
        // Keys does not actually support - the rules exist and nothing ever
        // selects them. This is the whole mapping, asserted.
        const auto expect = [](const QString& path,
                               SyntaxHighlighter::Language language) {
            QVERIFY2(SyntaxHighlighter::languageForPath(path) == language,
                     qPrintable(path));
        };

        expect(QStringLiteral("a.cs"), SyntaxHighlighter::Language::CSharp);
        expect(QStringLiteral("a.csx"), SyntaxHighlighter::Language::CSharp);
        expect(QStringLiteral("a.swift"), SyntaxHighlighter::Language::Swift);
        expect(QStringLiteral("a.php"), SyntaxHighlighter::Language::Php);
        expect(QStringLiteral("a.phtml"), SyntaxHighlighter::Language::Php);
        expect(QStringLiteral("a.kt"), SyntaxHighlighter::Language::Kotlin);
        expect(QStringLiteral("a.kts"), SyntaxHighlighter::Language::Kotlin);
        expect(QStringLiteral("a.dart"), SyntaxHighlighter::Language::Dart);
        expect(QStringLiteral("a.scala"), SyntaxHighlighter::Language::Scala);
        expect(QStringLiteral("a.lua"), SyntaxHighlighter::Language::Lua);
        expect(QStringLiteral("a.pl"), SyntaxHighlighter::Language::Perl);
        expect(QStringLiteral("a.pm"), SyntaxHighlighter::Language::Perl);
        expect(QStringLiteral("a.r"), SyntaxHighlighter::Language::R);

        // Extensions that map onto existing rules.
        expect(QStringLiteral("a.mjs"), SyntaxHighlighter::Language::JavaScript);
        expect(QStringLiteral("a.mts"), SyntaxHighlighter::Language::TypeScript);
        expect(QStringLiteral("a.cu"), SyntaxHighlighter::Language::C);
        expect(QStringLiteral("a.mm"), SyntaxHighlighter::Language::C);
        expect(QStringLiteral("a.vue"), SyntaxHighlighter::Language::Html);
        expect(QStringLiteral("a.less"), SyntaxHighlighter::Language::Css);
        expect(QStringLiteral("a.pyi"), SyntaxHighlighter::Language::Python);

        // Files whose name carries the type.
        expect(QStringLiteral("/p/Gemfile"), SyntaxHighlighter::Language::Ruby);
        expect(QStringLiteral("/p/.bashrc"), SyntaxHighlighter::Language::Shell);
        expect(QStringLiteral("/p/.editorconfig"), SyntaxHighlighter::Language::Toml);

        // And an extension with no rules stays None: mis-colouring reads as a
        // bug, no colour reads as a type Keys does not know yet.
        expect(QStringLiteral("a.zzz"), SyntaxHighlighter::Language::None);
    }

    void coloursCSharpKeywordsAndTypes()
    {
        QCOMPARE(kindOf(QStringLiteral("public sealed record Invoice(int Id);"),
                        QStringLiteral("sealed"),
                        SyntaxHighlighter::Language::CSharp),
                 TokenKind::Keyword);
        QCOMPARE(kindOf(QStringLiteral("public sealed record Invoice(int Id);"),
                        QStringLiteral("int"),
                        SyntaxHighlighter::Language::CSharp),
                 TokenKind::Type);
    }

    void coloursSwiftKeywords()
    {
        QCOMPARE(kindOf(QStringLiteral("guard let url = URL(string: s) else { return }"),
                        QStringLiteral("guard"),
                        SyntaxHighlighter::Language::Swift),
                 TokenKind::Keyword);
    }

    void coloursPhpKeywords()
    {
        QCOMPARE(kindOf(QStringLiteral("public function find(int $id): ?Invoice"),
                        QStringLiteral("function"),
                        SyntaxHighlighter::Language::Php),
                 TokenKind::Keyword);
    }

    void coloursKotlinKeywords()
    {
        QCOMPARE(kindOf(QStringLiteral("suspend fun find(id: Int): Invoice?"),
                        QStringLiteral("suspend"),
                        SyntaxHighlighter::Language::Kotlin),
                 TokenKind::Keyword);
    }

    void luaUsesDashDashComments()
    {
        // Not // - Lua would read that as two divisions, and the rest of the
        // line would stay plain while the reader expects a comment.
        QCOMPARE(kindOf(QStringLiteral("-- a comment"), QStringLiteral("-- a comment"),
                        SyntaxHighlighter::Language::Lua),
                 TokenKind::Comment);
    }

    void luaDoesNotTreatSlashStarAsAComment()
    {
        // The regression this guards: claiming C-style block comments for Lua
        // would make `a / *b` open a comment that never closes, greying out the
        // rest of the file.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::Lua);
        LineState outgoing = LineState::Normal;
        const std::vector<Token> tokens = highlighter.tokenize(
            QStringLiteral("local c = a / b"), LineState::Normal, outgoing);

        // Nothing should be a comment, and the line must not leave the parser
        // inside one.
        for (const Token& token : tokens) {
            QVERIFY(token.kind != TokenKind::Comment);
        }
        QCOMPARE(outgoing, LineState::Normal);
    }

    void perlAndRUseHashComments()
    {
        QCOMPARE(kindOf(QStringLiteral("# a comment"), QStringLiteral("# a comment"),
                        SyntaxHighlighter::Language::Perl),
                 TokenKind::Comment);
        QCOMPARE(kindOf(QStringLiteral("# a comment"), QStringLiteral("# a comment"),
                        SyntaxHighlighter::Language::R),
                 TokenKind::Comment);
    }

    void everyLanguageWithRulesProducesTokens()
    {
        // A language declared in the enum but wired to no keyword list would
        // silently highlight nothing. One representative line each.
        struct Sample {
            SyntaxHighlighter::Language language;
            QString line;
        };

        const std::vector<Sample> samples = {
            {SyntaxHighlighter::Language::CSharp,
             QStringLiteral("public class A { }")},
            {SyntaxHighlighter::Language::Swift,
             QStringLiteral("struct A { let x: Int }")},
            {SyntaxHighlighter::Language::Php,
             QStringLiteral("function a() { return null; }")},
            {SyntaxHighlighter::Language::Kotlin,
             QStringLiteral("class A(val x: Int)")},
            {SyntaxHighlighter::Language::Dart,
             QStringLiteral("class A { final int x = 0; }")},
            {SyntaxHighlighter::Language::Scala,
             QStringLiteral("case class A(x: Int)")},
            {SyntaxHighlighter::Language::Lua,
             QStringLiteral("local function a() return nil end")},
            {SyntaxHighlighter::Language::Perl,
             QStringLiteral("sub a { return 1; }")},
            {SyntaxHighlighter::Language::R,
             QStringLiteral("a <- function(x) { if (x > 0) TRUE else FALSE }")},
        };

        for (const Sample& sample : samples) {
            const SyntaxHighlighter highlighter(sample.language);
            LineState outgoing = LineState::Normal;
            const std::vector<Token> tokens =
                highlighter.tokenize(sample.line, LineState::Normal, outgoing);

            bool sawKeyword = false;
            for (const Token& token : tokens) {
                QVERIFY(token.start >= 0);
                QVERIFY(token.start + token.length <= sample.line.size());
                if (token.kind == TokenKind::Keyword) {
                    sawKeyword = true;
                }
            }
            QVERIFY2(sawKeyword,
                     qPrintable(QStringLiteral("no keyword found in: %1")
                                    .arg(sample.line)));
        }
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


    // ---- The wider token set -----------------------------------------------

    void coloursPreprocessorDirectivesApartFromKeywords()
    {
        // A header block is scaffolding around the code, not control flow, and
        // colouring it as a keyword makes the top of every file read as a wall.
        QCOMPARE(kindOf(QStringLiteral("#include <vector>"), QStringLiteral("#include"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Preprocessor);
        QCOMPARE(kindOf(QStringLiteral("#define MAX 10"), QStringLiteral("#define"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Preprocessor);
    }

    void aHashOutsideTheLeadingPositionIsNotADirective()
    {
        // `#` is only a directive at the start of a line. Anywhere else it is
        // an operator, and treating it otherwise would recolour the remainder.
        QVERIFY(kindOf(QStringLiteral("int a = b # c;"), QStringLiteral("#"), SyntaxHighlighter::Language::C)
                != TokenKind::Preprocessor);
    }

    void coloursLiteralValuesAsConstants()
    {
        // `true` and `nullptr` are reserved words, but they read as values
        // rather than actions - a condition is easier to scan when its
        // operands and its operators do not share a colour.
        QCOMPARE(kindOf(QStringLiteral("if (ready == true) {"), QStringLiteral("true"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Constant);
        QCOMPARE(kindOf(QStringLiteral("p = nullptr;"), QStringLiteral("nullptr"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Constant);
        QCOMPARE(kindOf(QStringLiteral("if x is None:"), QStringLiteral("None"),
                        SyntaxHighlighter::Language::Python),
                 TokenKind::Constant);
    }

    void coloursScreamingCaseAsAConstant()
    {
        QCOMPARE(kindOf(QStringLiteral("int n = MAX_SIZE;"), QStringLiteral("MAX_SIZE"),
                        SyntaxHighlighter::Language::C),
                 TokenKind::Constant);
    }

    void aSingleCapitalIsNotAConstant()
    {
        // One character is a type parameter or a loop variable far more often
        // than it is a constant.
        QVERIFY(kindOf(QStringLiteral("template <typename T> void f(T x);"), QStringLiteral("T"),
                       SyntaxHighlighter::Language::C)
                != TokenKind::Constant);
    }

    void separatesOperatorsFromBrackets()
    {
        QCOMPARE(kindOf(QStringLiteral("a += b;"), QStringLiteral("+="), SyntaxHighlighter::Language::C),
                 TokenKind::Operator);
        QCOMPARE(kindOf(QStringLiteral("f(a);"), QStringLiteral("("), SyntaxHighlighter::Language::C),
                 TokenKind::Punctuation);
    }

    void amultiCharacterOperatorIsOneToken()
    {
        // `!=` coloured character by character would flicker as it is typed.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::C);
        LineState outgoing = LineState::Normal;
        const std::vector<Token> tokens =
            highlighter.tokenize(QStringLiteral("if (a != b) {"), LineState::Normal, outgoing);

        const int at = static_cast<int>(QStringLiteral("if (a != b) {").indexOf(QStringLiteral("!=")));
        bool found = false;
        for (const Token& token : tokens) {
            if (token.start == at) {
                QCOMPARE(token.kind, TokenKind::Operator);
                QCOMPARE(token.length, 2);
                found = true;
            }
        }
        QVERIFY2(found, "the operator was not a token of its own");
    }

    // ---- The added languages -----------------------------------------------

    void detectsTheAddedFileTypes()
    {
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.java")),
                 SyntaxHighlighter::Language::Java);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.rb")),
                 SyntaxHighlighter::Language::Ruby);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.html")),
                 SyntaxHighlighter::Language::Html);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.css")),
                 SyntaxHighlighter::Language::Css);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.yml")),
                 SyntaxHighlighter::Language::Yaml);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.toml")),
                 SyntaxHighlighter::Language::Toml);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.sql")),
                 SyntaxHighlighter::Language::Sql);
    }

    void typeScriptGetsItsOwnKeywords()
    {
        // `.ts` used to be lexed as JavaScript, which left the type-level words
        // plain - the half of the language a TypeScript file is written for.
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("a.ts")),
                 SyntaxHighlighter::Language::TypeScript);
        QCOMPARE(kindOf(QStringLiteral("declare const x: string;"), QStringLiteral("declare"),
                        SyntaxHighlighter::Language::TypeScript),
                 TokenKind::Keyword);

        // And it still knows everything JavaScript does.
        QCOMPARE(kindOf(QStringLiteral("const x = 1;"), QStringLiteral("const"),
                        SyntaxHighlighter::Language::TypeScript),
                 TokenKind::Keyword);
    }

    void detectsFilesNamedRatherThanSuffixed()
    {
        // CMakeLists.txt has no usable extension, and it is the file a user of
        // this project opens most often after the sources themselves.
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("CMakeLists.txt")),
                 SyntaxHighlighter::Language::CMake);
        QCOMPARE(SyntaxHighlighter::languageForPath(
                     QStringLiteral("C:/project/CMakeLists.txt")),
                 SyntaxHighlighter::Language::CMake);
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("Dockerfile")),
                 SyntaxHighlighter::Language::Shell);
    }

    void aPlainTextFileIsStillLeftAlone()
    {
        // The name map must not turn every .txt into CMake.
        QCOMPARE(SyntaxHighlighter::languageForPath(QStringLiteral("notes.txt")),
                 SyntaxHighlighter::Language::None);
    }

    void coloursJavaAndRuby()
    {
        QCOMPARE(kindOf(QStringLiteral("public class Main {"), QStringLiteral("class"),
                        SyntaxHighlighter::Language::Java),
                 TokenKind::Keyword);
        QCOMPARE(kindOf(QStringLiteral("int count = 0;"), QStringLiteral("int"),
                        SyntaxHighlighter::Language::Java),
                 TokenKind::Type);
        QCOMPARE(kindOf(QStringLiteral("def greet(name)"), QStringLiteral("def"),
                        SyntaxHighlighter::Language::Ruby),
                 TokenKind::Keyword);
    }

    void coloursCMakeCommands()
    {
        QCOMPARE(kindOf(QStringLiteral("target_link_libraries(keys PRIVATE Qt6::Core)"),
                        QStringLiteral("target_link_libraries"), SyntaxHighlighter::Language::CMake),
                 TokenKind::Keyword);
    }

    void sqlKeywordsMatchInEitherCase()
    {
        // Real SQL is written both ways, and colouring only one is worse than
        // colouring neither.
        QCOMPARE(kindOf(QStringLiteral("SELECT * FROM users;"), QStringLiteral("SELECT"),
                        SyntaxHighlighter::Language::Sql),
                 TokenKind::Keyword);
        QCOMPARE(kindOf(QStringLiteral("select * from users;"), QStringLiteral("select"),
                        SyntaxHighlighter::Language::Sql),
                 TokenKind::Keyword);
    }

    void caseInsensitivityDoesNotLeakIntoOtherLanguages()
    {
        // `Return` is an identifier in C++, not a keyword.
        QVERIFY(kindOf(QStringLiteral("int Return = 1;"), QStringLiteral("Return"), SyntaxHighlighter::Language::C)
                != TokenKind::Keyword);
    }

    void sqlUsesItsOwnLineComment()
    {
        QCOMPARE(kindOf(QStringLiteral("select 1; -- a note"), QStringLiteral("-- a note"),
                        SyntaxHighlighter::Language::Sql),
                 TokenKind::Comment);
    }

    // ---- Markup ------------------------------------------------------------

    void coloursTagsAttributesAndValues()
    {
        const QString line = QStringLiteral("<div class=\"box\">text</div>");
        QCOMPARE(kindOf(line, QStringLiteral("<div"), SyntaxHighlighter::Language::Html), TokenKind::Tag);
        QCOMPARE(kindOf(line, QStringLiteral("class"), SyntaxHighlighter::Language::Html),
                 TokenKind::Attribute);
        QCOMPARE(kindOf(line, QStringLiteral("\"box\""), SyntaxHighlighter::Language::Html),
                 TokenKind::String);
    }

    void markupTextContentStaysPlain()
    {
        // Prose between tags is not code; running it through keyword rules
        // would colour ordinary English at random.
        QCOMPARE(kindOf(QStringLiteral("<p>return of the king</p>"), QStringLiteral("return"),
                        SyntaxHighlighter::Language::Html),
                 TokenKind::Plain);
    }

    void aMarkupCommentCarriesToTheNextLine()
    {
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::Html);
        LineState outgoing = LineState::Normal;
        std::ignore = highlighter.tokenize(QStringLiteral("<!-- opening"), LineState::Normal, outgoing);
        QCOMPARE(outgoing, LineState::InMarkupComment);

        // And the continuation line is all comment.
        QCOMPARE(kindOf(QStringLiteral("still inside"), QStringLiteral("still"), SyntaxHighlighter::Language::Html,
                        LineState::InMarkupComment),
                 TokenKind::Comment);
    }

    void aMarkupCommentDoesNotCloseOnABlockCommentTerminator()
    {
        // `*/` must not end an HTML comment: the two states are separate for a
        // reason, and sharing one would end the comment early.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::Html);
        LineState outgoing = LineState::Normal;
        std::ignore = highlighter.tokenize(QStringLiteral("nothing here */"), LineState::InMarkupComment,
                             outgoing);
        QCOMPARE(outgoing, LineState::InMarkupComment);
    }

    void anUnclosedTagCarriesToTheNextLine()
    {
        // An element with enough attributes to wrap is ordinary in HTML.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::Html);
        LineState outgoing = LineState::Normal;
        std::ignore = highlighter.tokenize(QStringLiteral("<div class=\"a\""), LineState::Normal, outgoing);
        QCOMPARE(outgoing, LineState::InTag);

        // The wrapped attribute is still an attribute, not prose.
        QCOMPARE(kindOf(QStringLiteral("      id=\"main\">"), QStringLiteral("id"), SyntaxHighlighter::Language::Html,
                        LineState::InTag),
                 TokenKind::Attribute);
    }

    // ---- CSS ---------------------------------------------------------------

    void coloursCssPropertiesAndValues()
    {
        const QString line = QStringLiteral("  color: #ff0000;");
        QCOMPARE(kindOf(line, QStringLiteral("color"), SyntaxHighlighter::Language::Css),
                 TokenKind::Attribute);
        QCOMPARE(kindOf(line, QStringLiteral("#ff0000"), SyntaxHighlighter::Language::Css),
                 TokenKind::Number);
    }

    void aClassSelectorIsOneToken()
    {
        // `.card` was emitting the dot alone and leaving the name plain, so a
        // class selector sat uncoloured beside a coloured `#id` on the same
        // line - visibly inconsistent in any real stylesheet.
        QCOMPARE(kindOf(QStringLiteral(".card, #main {"), QStringLiteral(".card"),
                        SyntaxHighlighter::Language::Css),
                 TokenKind::Tag);
        QCOMPARE(kindOf(QStringLiteral(".card, #main {"), QStringLiteral("card"),
                        SyntaxHighlighter::Language::Css),
                 TokenKind::Tag);
    }

    void coloursCssAtRules()
    {
        QCOMPARE(kindOf(QStringLiteral("@media (min-width: 600px) {"), QStringLiteral("@media"),
                        SyntaxHighlighter::Language::Css),
                 TokenKind::Keyword);
    }

    void cssUnitsStayWithTheirNumber()
    {
        // `600px` is one value; splitting it would give two colours to one
        // thing the reader sees as a single quantity.
        const SyntaxHighlighter highlighter(SyntaxHighlighter::Language::Css);
        LineState outgoing = LineState::Normal;
        const QString line = QStringLiteral("  width: 600px;");
        const std::vector<Token> tokens =
            highlighter.tokenize(line, LineState::Normal, outgoing);

        const int at = static_cast<int>(line.indexOf(QStringLiteral("600px")));
        bool found = false;
        for (const Token& token : tokens) {
            if (token.start == at) {
                QCOMPARE(token.length, 5);
                found = true;
            }
        }
        QVERIFY2(found, "the value and its unit were not one token");
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
