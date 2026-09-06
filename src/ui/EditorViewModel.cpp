#include "ui/EditorViewModel.h"

#include "ui/Theme.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>

using keys::editor::Position;
using keys::editor::Range;

namespace keys::ui {

EditorViewModel::EditorViewModel(EditorSettings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
}

void EditorViewModel::setDocument(editor::TextDocument* document)
{
    if (m_document == document) {
        return;
    }

    if (m_document) {
        m_document->disconnect(this);
    }

    m_document = document;

    // A document destroyed while the view still points at it would leave a
    // dangling pointer that the next setDocument dereferences. The workspace
    // rebinds panes before closing a document, so this should not happen - but
    // "should not" is not a guarantee, and the failure is a crash rather than
    // anything a user could report usefully.
    if (m_document) {
        connect(m_document, &QObject::destroyed, this, [this] {
            m_document = nullptr;
            m_lineStates.clear();
            m_highlighted = false;
            emit documentChanged();
        });
    }

    applyLanguage();

    if (m_document) {
        connect(m_document, &editor::TextDocument::contentsChanged, this,
                [this](const editor::Range& replaced, int) {
                    // Everything from the edited line onward may now start in a
                    // different state - opening a block comment recolours the
                    // rest of the file. Truncating rather than clearing keeps
                    // the lines above, which are unaffected.
                    if (m_lineStates.size() > static_cast<size_t>(replaced.start.line)) {
                        m_lineStates.resize(static_cast<size_t>(replaced.start.line));
                    }

                    // Before contentsChanged reaches QML, so a binding that
                    // reads `revision` sees the new value when it re-evaluates.
                    ++m_revision;
                });

        connect(m_document, &editor::TextDocument::contentsChanged,
                this, &EditorViewModel::contentsChanged);
        connect(m_document, &editor::TextDocument::cursorChanged,
                this, &EditorViewModel::cursorChanged);
        connect(m_document, &editor::TextDocument::modifiedChanged,
                this, &EditorViewModel::modifiedChanged);
        // The language follows the path, and the path can arrive *after* the
        // document is bound: EditorGroup creates the document, hands it to the
        // view, and only then sets its path. Re-detecting here is what makes
        // highlighting appear at all - computing it once in setDocument left
        // every file unhighlighted, because the path was still empty.
        connect(m_document, &editor::TextDocument::pathChanged, this,
                [this] {
                    applyLanguage();
                    emit documentChanged();
                });
    }

    ++m_revision;

    emit documentChanged();
    emit contentsChanged();
    emit cursorChanged();
    emit modifiedChanged();
}

int EditorViewModel::lineCount() const
{
    return m_document ? m_document->lineCount() : 0;
}

int EditorViewModel::cursorLine() const
{
    return m_document ? m_document->cursor().position.line : 0;
}

int EditorViewModel::cursorColumn() const
{
    return m_document ? m_document->cursor().position.column : 0;
}

bool EditorViewModel::hasSelection() const
{
    return m_document && m_document->cursor().hasSelection();
}

bool EditorViewModel::isModified() const
{
    return m_document && m_document->isModified();
}

QString EditorViewModel::path() const
{
    return m_document ? m_document->path() : QString();
}

QString EditorViewModel::fileName() const
{
    return m_document ? QFileInfo(m_document->path()).fileName() : QString();
}

QString EditorViewModel::lineText(int line) const
{
    return m_document ? m_document->line(line) : QString();
}

void EditorViewModel::applyLanguage()
{
    // The cached line states belong to whatever was highlighted before.
    m_lineStates.clear();

    m_highlighter.setLanguage(
        m_document ? editor::SyntaxHighlighter::languageForPath(m_document->path())
                   : editor::SyntaxHighlighter::Language::None);
    m_highlighted =
        m_highlighter.language() != editor::SyntaxHighlighter::Language::None;
}

QString EditorViewModel::colourFor(editor::TokenKind kind)
{
    // Read from the Theme rather than hard-coded, so a light/dark switch
    // recolours code with everything else. Theme is the application's instance;
    // see Theme::create.
    const Theme* theme = Theme::instance();
    if (!theme) {
        return QString();
    }

    switch (kind) {
    case editor::TokenKind::Keyword:
        return theme->synKeyword().name();
    case editor::TokenKind::Type:
        return theme->synType().name();
    case editor::TokenKind::String:
        return theme->synString().name();
    case editor::TokenKind::Number:
        return theme->synNumber().name();
    case editor::TokenKind::Comment:
        return theme->synComment().name();
    case editor::TokenKind::Function:
        return theme->synFunction().name();
    case editor::TokenKind::Punctuation:
        return theme->synPunct().name();
    case editor::TokenKind::Preprocessor:
        return theme->synPreproc().name();
    case editor::TokenKind::Constant:
        return theme->synConstant().name();
    case editor::TokenKind::Operator:
        return theme->synOperator().name();
    case editor::TokenKind::Tag:
        return theme->synTag().name();
    case editor::TokenKind::Attribute:
        return theme->synAttribute().name();
    case editor::TokenKind::Plain:
        break;
    }
    // Plain needs no span: the Text item's own colour already carries it.
    return QString();
}

QString EditorViewModel::languageName() const
{
    // Named for the reader, not for the enumerator: "C++" rather than "C", and
    // the family name where one lexer serves several dialects.
    switch (m_highlighter.language()) {
    case editor::SyntaxHighlighter::Language::C:          return QStringLiteral("C/C++");
    case editor::SyntaxHighlighter::Language::Python:     return QStringLiteral("Python");
    case editor::SyntaxHighlighter::Language::JavaScript: return QStringLiteral("JavaScript");
    case editor::SyntaxHighlighter::Language::TypeScript: return QStringLiteral("TypeScript");
    case editor::SyntaxHighlighter::Language::Rust:       return QStringLiteral("Rust");
    case editor::SyntaxHighlighter::Language::Go:         return QStringLiteral("Go");
    case editor::SyntaxHighlighter::Language::Qml:        return QStringLiteral("QML");
    case editor::SyntaxHighlighter::Language::Markdown:   return QStringLiteral("Markdown");
    case editor::SyntaxHighlighter::Language::Shell:      return QStringLiteral("Shell");
    case editor::SyntaxHighlighter::Language::Java:       return QStringLiteral("Java");
    case editor::SyntaxHighlighter::Language::Ruby:       return QStringLiteral("Ruby");
    case editor::SyntaxHighlighter::Language::Html:       return QStringLiteral("HTML");
    case editor::SyntaxHighlighter::Language::Css:        return QStringLiteral("CSS");
    case editor::SyntaxHighlighter::Language::Yaml:       return QStringLiteral("YAML");
    case editor::SyntaxHighlighter::Language::Toml:       return QStringLiteral("TOML");
    case editor::SyntaxHighlighter::Language::Sql:        return QStringLiteral("SQL");
    case editor::SyntaxHighlighter::Language::CMake:      return QStringLiteral("CMake");
    case editor::SyntaxHighlighter::Language::None:       break;
    }
    return QString();
}

QString EditorViewModel::highlightedLine(int line) const
{
    if (!m_document) {
        return QString();
    }

    const QString text = m_document->line(line);
    if (!m_highlighted || text.isEmpty()) {
        return text.toHtmlEscaped();
    }

    // The state this line starts in is the state the one before it ended in.
    // Cached, and filled in as lines are drawn - a viewport draws them in order,
    // so the entry is almost always already there. When it is not (a jump to the
    // middle of a file), the lines above are scanned once and then cached.
    if (m_lineStates.size() <= static_cast<size_t>(line)) {
        m_lineStates.resize(static_cast<size_t>(line) + 1,
                            editor::LineState::Normal);

        editor::LineState state = editor::LineState::Normal;
        for (int i = 0; i <= line; ++i) {
            m_lineStates[static_cast<size_t>(i)] = state;

            editor::LineState next = state;
            (void)m_highlighter.tokenize(m_document->line(i), state, next);
            state = next;
        }
    }

    editor::LineState outgoing = editor::LineState::Normal;
    const std::vector<editor::Token> tokens =
        m_highlighter.tokenize(text, m_lineStates[static_cast<size_t>(line)], outgoing);

    if (tokens.empty()) {
        return text.toHtmlEscaped();
    }

    // Built as one string with spans only where a token needs one. Plain runs
    // carry no markup, which keeps the common line short.
    QString html;
    html.reserve(text.size() * 2);

    int position = 0;
    for (const editor::Token& token : tokens) {
        if (token.start > position) {
            html += text.mid(position, token.start - position).toHtmlEscaped();
        }

        const QString colour = colourFor(token.kind);
        const QString body = text.mid(token.start, token.length).toHtmlEscaped();

        if (colour.isEmpty()) {
            html += body;
        } else {
            // <font color> rather than a CSS span: Text.StyledText supports only
            // a small HTML subset and ignores style attributes entirely, so a
            // span renders as plain text with the markup silently dropped. That
            // is exactly what shipped - the model was producing correct HTML
            // that the view could not read.
            html += QStringLiteral("<font color=\"%1\">%2</font>").arg(colour, body);
        }
        position = token.start + token.length;
    }

    if (position < text.size()) {
        html += text.mid(position).toHtmlEscaped();
    }
    return html;
}

bool EditorViewModel::lineHasSelection(int line) const
{
    if (!m_document || !m_document->cursor().hasSelection()) {
        return false;
    }
    const Range selection = m_document->cursor().selection();
    return line >= selection.start.line && line <= selection.end.line;
}

int EditorViewModel::selectionStartOn(int line) const
{
    if (!lineHasSelection(line)) {
        return 0;
    }
    const Range selection = m_document->cursor().selection();

    // Only the first line of a multi-line selection starts partway in; the rest
    // are highlighted from column zero.
    return line == selection.start.line ? selection.start.column : 0;
}

int EditorViewModel::selectionEndOn(int line) const
{
    if (!lineHasSelection(line)) {
        return 0;
    }
    const Range selection = m_document->cursor().selection();

    // Likewise, only the last line stops partway; earlier ones run to their end.
    // +1 past the line length on a wholly-selected line so the highlight covers
    // the newline, showing the user the line break is included.
    return line == selection.end.line ? selection.end.column
                                      : m_document->lineLength(line) + 1;
}

// ---- Input ------------------------------------------------------------------

void EditorViewModel::insertText(const QString& text)
{
    if (!m_document || text.isEmpty()) {
        return;
    }
    m_document->insertText(text);
    emit scrollToCursorRequested();
}

void EditorViewModel::insertNewline()
{
    if (!m_document) {
        return;
    }

    // Auto-indent: a new line starts at the same indentation as the one it came
    // from. Without this every line of a nested block has to be re-indented by
    // hand, which is the first thing anyone notices missing in an editor.
    const QString current = m_document->line(m_document->cursor().position.line);

    int indent = 0;
    while (indent < current.size() && (current.at(indent) == QLatin1Char(' ')
                                       || current.at(indent) == QLatin1Char('\t'))) {
        ++indent;
    }

    // Only carry indentation that is actually behind the caret: splitting a line
    // mid-indent should not duplicate what is already there.
    const int carry = std::min(indent, m_document->cursor().position.column);

    m_document->insertText(QLatin1String("\n") + current.left(carry));
    emit scrollToCursorRequested();
}

void EditorViewModel::insertTab()
{
    if (!m_document) {
        return;
    }
    m_document->insertText(m_settings.indentString());
    emit scrollToCursorRequested();
}

void EditorViewModel::deleteBackward()
{
    if (!m_document) {
        return;
    }
    m_document->deleteBackward();
    emit scrollToCursorRequested();
}

void EditorViewModel::deleteForward()
{
    if (!m_document) {
        return;
    }
    m_document->deleteForward();
    emit scrollToCursorRequested();
}

void EditorViewModel::moveCursor(int line, int column, bool extend)
{
    if (!m_document) {
        return;
    }
    m_document->setCursorPosition(Position{line, column}, extend);
    emit scrollToCursorRequested();
}

void EditorViewModel::moveLeft(bool extend, bool byWord)
{
    if (!m_document) {
        return;
    }
    byWord ? m_document->moveWordLeft(extend) : m_document->moveLeft(extend);
    emit scrollToCursorRequested();
}

void EditorViewModel::moveRight(bool extend, bool byWord)
{
    if (!m_document) {
        return;
    }
    byWord ? m_document->moveWordRight(extend) : m_document->moveRight(extend);
    emit scrollToCursorRequested();
}

void EditorViewModel::moveUp(bool extend)
{
    if (!m_document) {
        return;
    }
    m_document->moveUp(extend);
    emit scrollToCursorRequested();
}

void EditorViewModel::moveDown(bool extend)
{
    if (!m_document) {
        return;
    }
    m_document->moveDown(extend);
    emit scrollToCursorRequested();
}

void EditorViewModel::moveToLineStart(bool extend)
{
    if (!m_document) {
        return;
    }
    m_document->moveToLineStart(extend);
    emit scrollToCursorRequested();
}

void EditorViewModel::moveToLineEnd(bool extend)
{
    if (!m_document) {
        return;
    }
    m_document->moveToLineEnd(extend);
    emit scrollToCursorRequested();
}

void EditorViewModel::moveToDocumentStart(bool extend)
{
    if (!m_document) {
        return;
    }
    m_document->moveToDocumentStart(extend);
    emit scrollToCursorRequested();
}

void EditorViewModel::moveToDocumentEnd(bool extend)
{
    if (!m_document) {
        return;
    }
    m_document->moveToDocumentEnd(extend);
    emit scrollToCursorRequested();
}

void EditorViewModel::movePage(int lines, bool extend)
{
    if (!m_document || lines == 0) {
        return;
    }

    // Expressed as repeated line movement so the desired column is preserved
    // across the jump, exactly as it is for a single arrow press.
    const int steps = std::abs(lines);
    for (int i = 0; i < steps; ++i) {
        lines > 0 ? m_document->moveDown(extend) : m_document->moveUp(extend);
    }
    emit scrollToCursorRequested();
}

void EditorViewModel::selectAll()
{
    if (m_document) {
        m_document->selectAll();
    }
}

QString EditorViewModel::selectedText() const
{
    return m_document ? m_document->selectedText() : QString();
}

void EditorViewModel::undo()
{
    if (m_document && m_document->undo()) {
        emit scrollToCursorRequested();
    }
}

void EditorViewModel::redo()
{
    if (m_document && m_document->redo()) {
        emit scrollToCursorRequested();
    }
}

void EditorViewModel::copy()
{
    const QString text = selectedText();
    if (!text.isEmpty()) {
        QGuiApplication::clipboard()->setText(text);
    }
}

void EditorViewModel::cut()
{
    if (!m_document || !m_document->cursor().hasSelection()) {
        return;
    }
    copy();
    m_document->deleteBackward();   // removes the selection
    emit scrollToCursorRequested();
}

void EditorViewModel::paste()
{
    if (!m_document) {
        return;
    }
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty()) {
        return;
    }
    m_document->insertText(text);
    emit scrollToCursorRequested();
}

} // namespace keys::ui
