#include "editor/DocumentSearch.h"
#include "editor/TextDocument.h"

#include <QTest>

#include <tuple>

using namespace keys::editor;

/// Find and replace within a document.
///
/// The cases that matter are the ones a naive implementation gets wrong in a
/// way nobody notices until it loses someone's work: a regex that can match
/// nothing, a replacement that shifts every later match, wrapping at the ends
/// of the file.
class DocumentSearchTests : public QObject {
    Q_OBJECT

private:
    TextDocument m_document;
    DocumentSearch m_search;

    void load(const QString& text) { m_document.setText(text); }

    [[nodiscard]] int run(const QString& query, bool caseSensitive = false,
                          bool wholeWord = false, bool regex = false)
    {
        FindOptions options;
        options.query = query;
        options.caseSensitive = caseSensitive;
        options.wholeWord = wholeWord;
        options.regularExpression = regex;
        m_search.search(m_document, options);
        return m_search.count();
    }

private slots:
    void init()
    {
        m_document.setText(QString());
        m_search.clear();
    }

    // ---- Matching ----------------------------------------------------------

    void findsEveryOccurrenceIncludingSeveralOnOneLine()
    {
        load(QStringLiteral("foo bar foo\nbaz\nfoo"));
        QCOMPARE(run(QStringLiteral("foo")), 3);
    }

    void reportsPositionsAsLineAndColumn()
    {
        load(QStringLiteral("abc\nxxfooxx"));
        QCOMPARE(run(QStringLiteral("foo")), 1);

        const Range match = m_search.current();
        QCOMPARE(match.start.line, 1);
        QCOMPARE(match.start.column, 2);
        QCOMPARE(match.end.column, 5);
    }

    void isCaseInsensitiveByDefault()
    {
        load(QStringLiteral("Foo FOO foo"));
        QCOMPARE(run(QStringLiteral("foo")), 3);
        QCOMPARE(run(QStringLiteral("foo"), true), 1);
    }

    void wholeWordDoesNotMatchInsideAnIdentifier()
    {
        load(QStringLiteral("foo foobar barfoo"));
        QCOMPARE(run(QStringLiteral("foo")), 3);
        QCOMPARE(run(QStringLiteral("foo"), false, true), 1);
    }

    void aPlainQueryIsNotTreatedAsARegex()
    {
        // Someone typing `foo(` is looking for that text. Treating it as a
        // pattern would either match the wrong thing or fail to compile, and
        // both are worse than useless.
        load(QStringLiteral("call foo(x) and foo(y)"));
        QCOMPARE(run(QStringLiteral("foo(")), 2);

        load(QStringLiteral("a.b axb"));
        QCOMPARE(run(QStringLiteral("a.b")), 1);
    }

    void regexModeMatchesPatterns()
    {
        load(QStringLiteral("a1 b22 c333"));
        QCOMPARE(run(QStringLiteral("[0-9]+"), false, false, true), 3);
    }

    void anEmptyMatchingPatternIsIgnored()
    {
        // `a*` matches the empty string at every position. Reporting those
        // would give one "match" per character and no way to step past them.
        load(QStringLiteral("bbb"));
        QCOMPARE(run(QStringLiteral("a*"), false, false, true), 0);
    }

    void aMalformedRegexIsReportedRatherThanSilentlyMatchingNothing()
    {
        load(QStringLiteral("anything"));
        QCOMPARE(run(QStringLiteral("[unclosed"), false, false, true), 0);
        QVERIFY(!m_search.isQueryValid());

        // And a good query clears the flag again.
        QCOMPARE(run(QStringLiteral("any")), 1);
        QVERIFY(m_search.isQueryValid());
    }

    void anEmptyQueryClearsRatherThanMatchingEverything()
    {
        load(QStringLiteral("some text"));
        QCOMPARE(run(QString()), 0);
        QVERIFY(m_search.isQueryValid());
    }

    // ---- Navigation --------------------------------------------------------

    void nextWrapsAtTheEnd()
    {
        // Stopping at the last match reads as the find having broken.
        load(QStringLiteral("x\nx\nx"));
        QCOMPARE(run(QStringLiteral("x")), 3);

        QCOMPARE(m_search.currentIndex(), 0);
        m_search.next();
        m_search.next();
        QCOMPARE(m_search.currentIndex(), 2);
        m_search.next();
        QCOMPARE(m_search.currentIndex(), 0);
    }

    void previousWrapsAtTheStart()
    {
        load(QStringLiteral("x\nx\nx"));
        std::ignore = run(QStringLiteral("x"));

        QCOMPARE(m_search.currentIndex(), 0);
        m_search.previous();
        QCOMPARE(m_search.currentIndex(), 2);
    }

    void navigationOnNoMatchesDoesNothing()
    {
        load(QStringLiteral("abc"));
        QCOMPARE(run(QStringLiteral("zzz")), 0);

        m_search.next();
        m_search.previous();
        QCOMPARE(m_search.currentIndex(), -1);
        QVERIFY(m_search.current().isEmpty());
    }

    void selectNearestContinuesFromTheCaret()
    {
        // Opening the find bar should carry on from where the user is looking,
        // not send them to the top of the file to find their place again.
        load(QStringLiteral("x\nx\nx\nx"));
        std::ignore = run(QStringLiteral("x"));

        m_search.selectNearest(Position{2, 0});
        QCOMPARE(m_search.currentIndex(), 2);
    }

    void selectNearestWrapsWhenEveryMatchIsAbove()
    {
        load(QStringLiteral("x\nx\nplain"));
        std::ignore = run(QStringLiteral("x"));

        m_search.selectNearest(Position{2, 0});
        QCOMPARE(m_search.currentIndex(), 0);
    }

    // ---- Replacement -------------------------------------------------------

    void replacingOneMatchLeavesTheRestIntact()
    {
        load(QStringLiteral("foo foo foo"));
        std::ignore = run(QStringLiteral("foo"));

        const Range first = m_search.current();
        m_document.replaceRange(first, QStringLiteral("bar"));

        QCOMPARE(m_document.text(), QStringLiteral("bar foo foo"));
    }

    void replacingAllIsOneUndoStep()
    {
        // One action as far as the user is concerned. Forty presses of Ctrl+Z
        // to take back one replace-all would be absurd.
        load(QStringLiteral("a a a a"));
        std::ignore = run(QStringLiteral("a"));

        m_document.replaceRange(
            Range{Position{0, 0}, m_document.buffer().endPosition()},
            QStringLiteral("b b b b"));
        QCOMPARE(m_document.text(), QStringLiteral("b b b b"));

        QVERIFY(m_document.undo());
        QCOMPARE(m_document.text(), QStringLiteral("a a a a"));
    }

    void aReplacementOfDifferentLengthDoesNotCorruptLaterText()
    {
        // Replacing forwards would shift every later match; the longer the
        // replacement, the further wrong each one lands.
        load(QStringLiteral("x x x"));
        std::ignore = run(QStringLiteral("x"));

        const std::vector<Range> matches = m_search.matches();
        QCOMPARE(static_cast<int>(matches.size()), 3);

        const keys::editor::TextBuffer& buffer = m_document.buffer();
        QString text = m_document.text();
        for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
            const int from = buffer.offsetOf(it->start);
            const int to = buffer.offsetOf(it->end);
            text.replace(from, to - from, QStringLiteral("LONG"));
        }

        QCOMPARE(text, QStringLiteral("LONG LONG LONG"));
    }
};

QTEST_MAIN(DocumentSearchTests)
#include "DocumentSearchTests.moc"
