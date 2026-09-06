#include "config/Settings.h"
#include "editor/TextDocument.h"
#include "ui/EditorSettings.h"
#include "ui/EditorViewModel.h"

#include <QTest>

#include <memory>

using keys::config::Settings;
using keys::editor::Position;
using keys::editor::TextDocument;
using keys::ui::EditorSettings;
using keys::ui::EditorViewModel;

/// Bracket matching, and the indentation that goes with it.
///
/// The cases worth testing are the ones a naive matcher gets wrong: nesting,
/// where the answer is not the next bracket of the right kind; a missing
/// partner, which must be reported rather than searched for forever; and the
/// caret sitting after a bracket rather than on it, which is where it is when
/// you have just typed one.
class BracketTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<EditorSettings> m_editorSettings;
    std::unique_ptr<EditorViewModel> m_model;
    std::unique_ptr<TextDocument> m_document;

    void load(const QString& text)
    {
        m_model->setDocument(nullptr);
        m_document = std::make_unique<TextDocument>();
        m_document->setText(text);
        m_model->setDocument(m_document.get());
    }

    void putCaret(int line, int column)
    {
        m_document->setCursorPosition(Position{line, column});
    }

private slots:
    void init()
    {
        m_settings = std::make_unique<Settings>();
        m_editorSettings = std::make_unique<EditorSettings>(*m_settings);
        m_model = std::make_unique<EditorViewModel>(*m_editorSettings);
    }

    void cleanup()
    {
        m_model.reset();
        m_document.reset();
        m_editorSettings.reset();
        m_settings.reset();
    }

    // ---- Matching ----------------------------------------------------------

    void matchesAPairOnOneLine()
    {
        load(QStringLiteral("call(arg)"));
        putCaret(0, 4);   // on the '('

        QVERIFY(m_model->isBracketMatched());
        QCOMPARE(m_model->matchLine(), 0);
        QCOMPARE(m_model->matchColumn(), 8);
    }

    void matchesFromTheClosingSide()
    {
        load(QStringLiteral("call(arg)"));
        putCaret(0, 8);   // on the ')'

        QVERIFY(m_model->isBracketMatched());
        QCOMPARE(m_model->matchColumn(), 4);
    }

    void matchesTheBracketJustTyped()
    {
        // After typing '(' the caret is past it, not on it. If only the
        // character at the caret counted, the highlight would never appear
        // while typing - which is when it is most useful.
        load(QStringLiteral("call(arg)"));
        putCaret(0, 5);   // just after the '('

        QVERIFY(m_model->isBracketMatched());
        QCOMPARE(m_model->matchColumn(), 8);
    }

    void respectsNesting()
    {
        // The partner of the outer brace is the last one, not the first found.
        load(QStringLiteral("{ { } }"));
        putCaret(0, 0);

        QVERIFY(m_model->isBracketMatched());
        QCOMPARE(m_model->matchColumn(), 6);
    }

    void matchesAcrossLines()
    {
        load(QStringLiteral("void f()\n{\n    body();\n}"));
        putCaret(1, 0);   // the opening brace on its own line

        QVERIFY(m_model->isBracketMatched());
        QCOMPARE(m_model->matchLine(), 3);
        QCOMPARE(m_model->matchColumn(), 0);
    }

    void reportsAnUnmatchedBracket()
    {
        // Reported as unmatched rather than as no bracket at all, so the view
        // can mark it - which is the fastest way to find a missing brace.
        load(QStringLiteral("void f() {\n    body();"));
        putCaret(0, 9);

        QCOMPARE(m_model->bracketColumn(), 9);
        QVERIFY(!m_model->isBracketMatched());
    }

    void ignoresACaretThatIsNotOnABracket()
    {
        load(QStringLiteral("plain text"));
        putCaret(0, 3);

        QCOMPARE(m_model->bracketLine(), -1);
        QVERIFY(!m_model->isBracketMatched());
    }

    void doesNotConfuseDifferentBracketKinds()
    {
        load(QStringLiteral("f(a[b])"));
        putCaret(0, 1);   // the '('

        QVERIFY(m_model->isBracketMatched());
        QCOMPARE(m_model->matchColumn(), 6);
    }

    // ---- Indentation -------------------------------------------------------

    void newlineCarriesTheIndentation()
    {
        load(QStringLiteral("    indented"));
        putCaret(0, 12);
        m_model->insertNewline();

        QCOMPARE(m_document->line(1), QStringLiteral("    "));
    }

    void newlineAfterAnOpeningBraceGoesOneDeeper()
    {
        load(QStringLiteral("void f() {"));
        putCaret(0, 10);
        m_model->insertNewline();

        // Four spaces is the default indent; the point is that it is deeper
        // than the line above, which had none.
        QVERIFY2(m_document->line(1).size() > 0,
                 qPrintable(QStringLiteral("got '%1'").arg(m_document->line(1))));
        QVERIFY(m_document->line(1).trimmed().isEmpty());
    }

    void aClosingBraceOnItsOwnLinePullsBack()
    {
        load(QStringLiteral("void f() {\n    "));
        putCaret(1, 4);
        m_model->insertText(QStringLiteral("}"));

        // The brace lands under the `void`, not one level in from it.
        QCOMPARE(m_document->line(1), QStringLiteral("}"));
    }

    void aBraceAfterCodeIsNotPulledBack()
    {
        // `};` at the end of a line closes an initialiser, not a block, and
        // moving it would be wrong.
        load(QStringLiteral("    int a[] = {1, 2"));
        putCaret(0, 19);
        m_model->insertText(QStringLiteral("}"));

        QCOMPARE(m_document->line(0), QStringLiteral("    int a[] = {1, 2}"));
    }
};

QTEST_MAIN(BracketTests)
#include "BracketTests.moc"
