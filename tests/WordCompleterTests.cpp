#include "editor/SyntaxHighlighter.h"
#include "editor/TextDocument.h"
#include "editor/WordCompleter.h"

#include <QTest>

#include <memory>

using keys::editor::SyntaxHighlighter;
using keys::editor::TextDocument;
using keys::editor::WordCompleter;
using keys::editor::WordSuggestion;

/// Completion without a language server.
///
/// The gap this closes: completion came only from a language server, so on a
/// machine with no clangd, pyright or gopls - which is most machines - typing
/// offered nothing at all.
///
/// What matters here is that it never guesses. Every suggestion is a word that
/// already exists, in the buffer or in the language's reserved words, because a
/// confidently wrong completion is worse than none.
class WordCompleterTests : public QObject {
    Q_OBJECT

private:
    [[nodiscard]] static std::unique_ptr<TextDocument> documentOf(const QString& text)
    {
        auto document = std::make_unique<TextDocument>();
        document->setText(text);
        return document;
    }

    [[nodiscard]] static QStringList wordsOf(
        const std::vector<WordSuggestion>& suggestions)
    {
        QStringList words;
        for (const WordSuggestion& suggestion : suggestions) {
            words << suggestion.word;
        }
        return words;
    }

private slots:
    // ---- What counts as a prefix ------------------------------------------

    void readsThePrefixBeforeTheCaret()
    {
        QCOMPARE(WordCompleter::prefixAt(QStringLiteral("const value"), 11),
                 QStringLiteral("value"));
        QCOMPARE(WordCompleter::prefixAt(QStringLiteral("const value"), 8),
                 QStringLiteral("va"));
    }

    void aCaretNotInAWordHasNoPrefix()
    {
        // Column 6 is the space between the two words; column 5 is the end of
        // `const`, where the prefix is `const` rather than nothing.
        QVERIFY(WordCompleter::prefixAt(QStringLiteral("const value"), 6).isEmpty());
        QVERIFY(WordCompleter::prefixAt(QStringLiteral(""), 0).isEmpty());
        QCOMPARE(WordCompleter::prefixAt(QStringLiteral("const value"), 5),
                 QStringLiteral("const"));
    }

    void aNumberIsNotAPrefix()
    {
        // Completing `42` against every word starting with 4 is noise, and a
        // run beginning with a digit is a literal rather than an identifier.
        QVERIFY(WordCompleter::prefixAt(QStringLiteral("x = 42"), 6).isEmpty());
        QVERIFY(WordCompleter::prefixAt(QStringLiteral("x = 0xFF"), 8).isEmpty());
    }

    void anUnderscoreStartsAWord()
    {
        QCOMPARE(WordCompleter::prefixAt(QStringLiteral("_private"), 8),
                 QStringLiteral("_private"));
    }

    // ---- Words in the buffer ----------------------------------------------

    void collectsIdentifierShapedWords()
    {
        const QStringList lines = {
            QStringLiteral("const invoiceTotal = 42;"),
            QStringLiteral("send(invoiceTotal);"),
        };

        QStringList found;
        for (const auto& [word, line] : WordCompleter::wordsIn(lines)) {
            found << word;
        }
        found.sort();

        QVERIFY(found.contains(QStringLiteral("invoiceTotal")));
        QVERIFY(found.contains(QStringLiteral("const")));
        QVERIFY(found.contains(QStringLiteral("send")));

        // Numbers are not words.
        QVERIFY(!found.contains(QStringLiteral("42")));
    }

    void singleCharactersAreNotCollected()
    {
        // `i`, `x` and `n` are the most common words in any file and the least
        // useful to complete.
        const QStringList lines = {QStringLiteral("for (int i = 0; i < n; ++i)")};

        QStringList found;
        for (const auto& [word, line] : WordCompleter::wordsIn(lines)) {
            found << word;
        }

        QVERIFY(!found.contains(QStringLiteral("i")));
        QVERIFY(!found.contains(QStringLiteral("n")));
        QVERIFY(found.contains(QStringLiteral("for")));
        QVERIFY(found.contains(QStringLiteral("int")));
    }

    void suggestsAWordAlreadyInTheFile()
    {
        const auto document = documentOf(
            QStringLiteral("const invoiceTotal = 42;\ninv"));

        const std::vector<WordSuggestion> suggestions =
            WordCompleter::suggest(*document, 1, 3,
                                   SyntaxHighlighter::Language::None);

        QVERIFY(wordsOf(suggestions).contains(QStringLiteral("invoiceTotal")));
    }

    void theWordBeingTypedIsNotSuggestedForItself()
    {
        const auto document = documentOf(QStringLiteral("value\nvalue"));

        const std::vector<WordSuggestion> suggestions =
            WordCompleter::suggest(*document, 1, 5,
                                   SyntaxHighlighter::Language::None);

        QVERIFY(!wordsOf(suggestions).contains(QStringLiteral("value")));
    }

    void aShortPrefixSuggestsNothing()
    {
        // One character matches most of the file, and the popup becomes noise.
        const auto document = documentOf(QStringLiteral("invoiceTotal\ni"));

        const std::vector<WordSuggestion> suggestions =
            WordCompleter::suggest(*document, 1, 1,
                                   SyntaxHighlighter::Language::None);

        QVERIFY(suggestions.empty());
    }

    // ---- Ranking -----------------------------------------------------------

    void nearerWordsComeFirst()
    {
        // A word used two lines up is far more likely to be the one wanted than
        // the same shape four hundred lines away.
        QStringList lines;
        lines << QStringLiteral("alphaFar = 1;");
        for (int i = 0; i < 40; ++i) {
            lines << QString();
        }
        lines << QStringLiteral("alphaNear = 2;");
        lines << QStringLiteral("alp");

        const auto document = documentOf(lines.join(QLatin1Char('\n')));

        const std::vector<WordSuggestion> suggestions = WordCompleter::suggest(
            *document, lines.size() - 1, 3, SyntaxHighlighter::Language::None);

        const QStringList words = wordsOf(suggestions);
        QVERIFY(words.indexOf(QStringLiteral("alphaNear"))
                < words.indexOf(QStringLiteral("alphaFar")));
    }

    void aNearbyBufferWordOutranksAKeyword()
    {
        const auto document = documentOf(
            QStringLiteral("constellation = 1;\ncons"));

        const std::vector<WordSuggestion> suggestions =
            WordCompleter::suggest(*document, 1, 4,
                                   SyntaxHighlighter::Language::C);

        const QStringList words = wordsOf(suggestions);
        QVERIFY(words.contains(QStringLiteral("constellation")));
        QVERIFY(words.contains(QStringLiteral("const")));
        QVERIFY(words.indexOf(QStringLiteral("constellation"))
                < words.indexOf(QStringLiteral("const")));
    }

    // ---- The language's own vocabulary -------------------------------------

    void suggestsKeywordsOfTheLanguage()
    {
        const auto document = documentOf(QStringLiteral("stat"));

        const std::vector<WordSuggestion> suggestions =
            WordCompleter::suggest(*document, 0, 4,
                                   SyntaxHighlighter::Language::C);

        QVERIFY(wordsOf(suggestions).contains(QStringLiteral("static")));
    }

    void suggestsTypesOfTheLanguage()
    {
        // A type Rust actually has. `uint` is C's spelling, not Rust's, and
        // asserting against a type the language does not define tested nothing.
        const auto document = documentOf(QStringLiteral("u3"));

        const std::vector<WordSuggestion> suggestions =
            WordCompleter::suggest(*document, 0, 2,
                                   SyntaxHighlighter::Language::Rust);

        const QStringList words = wordsOf(suggestions);
        QVERIFY2(words.contains(QStringLiteral("u32")),
                 qPrintable(words.join(QLatin1Char(' '))));
    }

    void noLanguageMeansBufferWordsOnly()
    {
        // A file Keys has no rules for still completes from itself, and offers
        // no other language's keywords.
        const auto document = documentOf(QStringLiteral("something\nsome"));

        const std::vector<WordSuggestion> suggestions =
            WordCompleter::suggest(*document, 1, 4,
                                   SyntaxHighlighter::Language::None);

        const QStringList words = wordsOf(suggestions);
        QVERIFY(words.contains(QStringLiteral("something")));
        for (const WordSuggestion& suggestion : suggestions) {
            QCOMPARE(suggestion.source, WordSuggestion::Source::Buffer);
        }
    }

    void aWordIsOfferedOnceEvenWhenItIsAlsoAKeyword()
    {
        // `static` in a C file is both a keyword and a word in the buffer.
        // Offering it twice would look like a bug.
        const auto document = documentOf(
            QStringLiteral("static int a;\nstat"));

        const std::vector<WordSuggestion> suggestions =
            WordCompleter::suggest(*document, 1, 4,
                                   SyntaxHighlighter::Language::C);

        QCOMPARE(wordsOf(suggestions).count(QStringLiteral("static")), 1);
    }

    // ---- Bounds -------------------------------------------------------------

    void theListIsBounded()
    {
        // A popup longer than this is not read, it is scrolled past.
        QStringList lines;
        for (int i = 0; i < 300; ++i) {
            lines << QStringLiteral("prefixWord%1 = %1;").arg(i);
        }
        lines << QStringLiteral("prefixWord");

        const auto document = documentOf(lines.join(QLatin1Char('\n')));

        const std::vector<WordSuggestion> suggestions = WordCompleter::suggest(
            *document, lines.size() - 1, 10, SyntaxHighlighter::Language::None);

        QVERIFY(static_cast<int>(suggestions.size())
                <= WordCompleter::kMaxSuggestions);
    }

    void anOutOfRangeLineIsSafe()
    {
        const auto document = documentOf(QStringLiteral("value"));

        QVERIFY(WordCompleter::suggest(*document, -1, 0,
                                       SyntaxHighlighter::Language::None).empty());
        QVERIFY(WordCompleter::suggest(*document, 99, 0,
                                       SyntaxHighlighter::Language::None).empty());
    }

    void anEmptyDocumentSuggestsNothing()
    {
        const auto document = documentOf(QString());

        QVERIFY(WordCompleter::suggest(*document, 0, 0,
                                       SyntaxHighlighter::Language::None).empty());
    }
};

QTEST_MAIN(WordCompleterTests)
#include "WordCompleterTests.moc"
