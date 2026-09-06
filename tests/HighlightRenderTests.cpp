#include "config/Settings.h"
#include "editor/TextDocument.h"
#include "ui/EditorSettings.h"
#include "ui/EditorViewModel.h"
#include "ui/Theme.h"

#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <tuple>

using keys::config::Settings;
using keys::editor::TextDocument;
using keys::ui::EditorSettings;
using keys::ui::EditorViewModel;
using keys::ui::Theme;

/// The rich text the editor actually renders.
///
/// The tokeniser has its own tests; this covers the layer above it, where a
/// different class of bug lives. Source code is full of characters that are
/// markup in HTML - `<`, `&`, `"` - and one that escapes through turns the rest
/// of the line into garbage or swallows it entirely. That cannot be caught by
/// testing tokens.
class HighlightRenderTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<Theme> m_theme;
    std::unique_ptr<EditorSettings> m_editorSettings;
    std::unique_ptr<EditorViewModel> m_model;
    std::unique_ptr<TextDocument> m_document;

    /// Renders one line of a document with the given path, so the language is
    /// chosen the way it is in the application.
    [[nodiscard]] QString render(const QString& path, const QString& text)
    {
        // Unbound before the old document is destroyed. The application rebinds
        // panes before closing a document; a test that skipped this would be
        // exercising a sequence the workspace never produces.
        m_model->setDocument(nullptr);

        m_document = std::make_unique<TextDocument>();
        m_document->setText(text);
        m_document->setPath(path);

        m_model->setDocument(m_document.get());
        return m_model->highlightedLine(0);
    }

private slots:
    void init()
    {
        m_settings = std::make_unique<Settings>();

        // The colours come from the Theme, so one has to be published or every
        // token renders without a span.
        m_theme = std::make_unique<Theme>(*m_settings);
        Theme::setInstance(m_theme.get());

        m_editorSettings = std::make_unique<EditorSettings>(*m_settings);
        m_model = std::make_unique<EditorViewModel>(*m_editorSettings);
    }

    void cleanup()
    {
        Theme::setInstance(nullptr);
        m_model.reset();
        m_document.reset();
        m_editorSettings.reset();
        m_theme.reset();
        m_settings.reset();
    }

    // ---- Escaping ----------------------------------------------------------

    void escapesAngleBracketsInCode()
    {
        // `std::vector<int>` must not become a `<int>` element. This is the bug
        // that eats the rest of the line and shows nothing at all.
        const QString html =
            render(QStringLiteral("a.cpp"), QStringLiteral("std::vector<int> values;"));

        QVERIFY2(html.contains(QStringLiteral("&lt;")), qPrintable(html));
        QVERIFY2(html.contains(QStringLiteral("&gt;")), qPrintable(html));

        // The text is still all there.
        QVERIFY2(html.contains(QStringLiteral("values")), qPrintable(html));
    }

    void escapesAmpersands()
    {
        const QString html =
            render(QStringLiteral("a.cpp"), QStringLiteral("if (a && b) return;"));
        QVERIFY2(html.contains(QStringLiteral("&amp;")), qPrintable(html));
    }

    void escapesInsideAColouredToken()
    {
        // A string literal containing markup is escaped *and* coloured. Getting
        // the order wrong escapes the span's own tags.
        const QString html = render(QStringLiteral("a.cpp"),
                                    QStringLiteral("s = \"<b>bold</b>\";"));

        QVERIFY2(html.contains(QStringLiteral("&lt;b&gt;")), qPrintable(html));
        // The span is real markup and must survive.
        QVERIFY2(html.contains(QStringLiteral("<font")), qPrintable(html));
    }

    void escapesAnUnhighlightedLineToo()
    {
        // A file type with no rules still goes through Text.StyledText only when
        // highlighted() is true - but the plain path must escape as well, or a
        // future change to that flag would silently produce markup injection.
        const QString html =
            render(QStringLiteral("notes.xyz"), QStringLiteral("a < b & c > d"));

        QVERIFY2(html.contains(QStringLiteral("&lt;")), qPrintable(html));
        QVERIFY2(html.contains(QStringLiteral("&amp;")), qPrintable(html));
        QVERIFY2(!html.contains(QStringLiteral("<font")), qPrintable(html));
    }

    // ---- Colouring ---------------------------------------------------------

    void coloursAKeyword()
    {
        const QString html =
            render(QStringLiteral("a.cpp"), QStringLiteral("return value;"));

        QVERIFY2(html.contains(QStringLiteral("<font color=\"")), qPrintable(html));
        QVERIFY2(html.contains(QStringLiteral("return")), qPrintable(html));
    }

    void leavesPlainTextWithoutSpans()
    {
        // A plain run carries no markup, which keeps the common line short - a
        // span per character would be far more text than the code itself.
        const QString html =
            render(QStringLiteral("a.cpp"), QStringLiteral("someIdentifier"));
        QCOMPARE(html, QStringLiteral("someIdentifier"));
    }

    void reportsWhetherAFileIsHighlighted()
    {
        (void)render(QStringLiteral("a.cpp"), QStringLiteral("int x;"));
        QVERIFY(m_model->isHighlighted());

        (void)render(QStringLiteral("notes.xyz"), QStringLiteral("int x;"));
        QVERIFY(!m_model->isHighlighted());
    }

    void detectsTheLanguageWhenThePathArrivesAfterTheDocument()
    {
        // The order the workspace actually uses: EditorGroup creates the
        // document, binds it to the view, and only then sets its path. Detecting
        // the language once at bind time leaves every file unhighlighted,
        // because the path is still empty - which is exactly what shipped and
        // what no test caught, since the others set the path first.
        m_model->setDocument(nullptr);

        m_document = std::make_unique<TextDocument>();
        m_document->setText(QStringLiteral("return value;"));

        m_model->setDocument(m_document.get());
        QVERIFY(!m_model->isHighlighted());   // no path yet

        m_document->setPath(QStringLiteral("a.cpp"));
        QVERIFY2(m_model->isHighlighted(),
                 "the language must be re-detected when the path arrives");

        QVERIFY(m_model->highlightedLine(0).contains(QStringLiteral("<font")));
    }

    // ---- Content preservation ----------------------------------------------

    void everyCharacterSurvivesRendering()
    {
        // The strongest check: strip the markup back out and the original line
        // must be exactly what went in. A token with a wrong offset would drop
        // or duplicate characters, which no single assertion above would catch.
        const QStringList lines = {
            QStringLiteral("int x = 1; // a comment"),
            QStringLiteral("s = \"a\\\"b\" + 'c';"),
            QStringLiteral("if (a < b && c > d) { return f(x); }"),
            QStringLiteral("auto v = std::vector<std::pair<int, QString>>{};"),
            QStringLiteral("    /* block */ value += 0xFF;"),
            QStringLiteral("&<>\"'"),
        };

        for (const QString& line : lines) {
            const QString html = render(QStringLiteral("a.cpp"), line);

            // Remove spans, then unescape.
            QString plain = html;
            plain.remove(QRegularExpression(QStringLiteral("<font[^>]*>")));
            plain.remove(QStringLiteral("</font>"));
            plain.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
            plain.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
            plain.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
            plain.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
            plain.replace(QStringLiteral("&amp;"), QStringLiteral("&"));

            // Indentation is carried as non-breaking spaces: StyledText
            // collapses ordinary runs of whitespace the way HTML does, which
            // rendered every indented line flush left.
            plain.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));

            QCOMPARE(plain, line);
        }
    }

    void anEmptyLineRendersAsNothing()
    {
        QCOMPARE(render(QStringLiteral("a.cpp"), QString()), QString());
    }

    // ---- Change notification ----------------------------------------------

    /// An edit must move `revision`.
    ///
    /// `lineText` and `highlightedLine` are functions, so a QML binding that
    /// calls one has nothing to depend on and is evaluated exactly once. The
    /// view bindings name `revision` to get a dependency the engine tracks.
    /// If it stops changing, typing silently stops repainting - the buffer
    /// moves on while the screen keeps showing the old line, which is what
    /// happened before this existed and what no tokeniser test could catch.
    void editingMovesTheRevision()
    {
        std::ignore = render(QStringLiteral("a.cpp"), QStringLiteral("int a;"));

        const int before = m_model->revision();
        QSignalSpy spy(m_model.get(), &EditorViewModel::contentsChanged);

        m_document->insertText(QStringLiteral("x"));

        QVERIFY2(m_model->revision() != before,
                 "an edit left the revision unchanged, so bindings will not "
                 "re-evaluate and the view will show stale text");
        QCOMPARE(spy.count(), 1);
    }

    /// The revision must be new *before* the signal is delivered, or a binding
    /// re-evaluating in response reads the previous value and the whole
    /// mechanism buys nothing.
    void theRevisionIsCurrentWhenTheSignalArrives()
    {
        std::ignore = render(QStringLiteral("a.cpp"), QStringLiteral("int a;"));

        const int before = m_model->revision();
        int observed = before;
        connect(m_model.get(), &EditorViewModel::contentsChanged, this,
                [this, &observed] { observed = m_model->revision(); });

        m_document->insertText(QStringLiteral("x"));

        QVERIFY2(observed != before,
                 "contentsChanged was emitted while revision still held its "
                 "old value");
    }

    /// Binding a different document is a content change too: the line the view
    /// is showing belongs to a file that is no longer open.
    void bindingADocumentMovesTheRevision()
    {
        std::ignore = render(QStringLiteral("a.cpp"), QStringLiteral("int a;"));
        const int before = m_model->revision();

        std::ignore = render(QStringLiteral("b.cpp"), QStringLiteral("int b;"));

        QVERIFY(m_model->revision() != before);
    }
};

QTEST_MAIN(HighlightRenderTests)
#include "HighlightRenderTests.moc"
