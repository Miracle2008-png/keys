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

                    // Fold regions follow the text. Rebuilding is one pass
                    // over the line lengths, which is cheap enough to do on
                    // every change and leaves no chance of a stale region.
                    if (m_document) {
                        m_folds.rebuild(*m_document);
                    }

                    // Before contentsChanged reaches QML, so a binding that
                    // reads `revision` sees the new value when it re-evaluates.
                    ++m_revision;
                });

        connect(m_document, &editor::TextDocument::contentsChanged,
                this, &EditorViewModel::contentsChanged);
        connect(m_document, &editor::TextDocument::cursorChanged, this, [this] {
            updateBracketMatch();
            emit cursorChanged();
        });
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

    if (m_document) {
        m_folds.clear();
        m_folds.rebuild(*m_document);
    } else {
        m_folds.clear();
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

// ---- Find and replace ------------------------------------------------------

void EditorViewModel::openFind(bool withReplace)
{
    m_findOpen = true;
    m_replaceOpen = withReplace;

    // A selection is almost always what the user means to search for, so
    // opening the bar with text selected fills the field with it. Only a
    // single-line selection: a multi-line one is a block being moved, not a
    // term being looked for.
    if (m_document) {
        const editor::Range selection = m_document->cursor().selection();
        if (!selection.isEmpty() && selection.start.line == selection.end.line) {
            const QString line = m_document->line(selection.start.line);
            m_findOptions.query =
                line.mid(selection.start.column,
                         selection.end.column - selection.start.column);
        }
    }

    refreshFind(true);
    emit findChanged();
}

void EditorViewModel::closeFind()
{
    if (!m_findOpen) {
        return;
    }
    m_findOpen = false;
    m_replaceOpen = false;
    m_find.clear();

    // The highlights disappear with the bar, so the view has to repaint.
    ++m_revision;
    emit contentsChanged();
    emit findChanged();
}

void EditorViewModel::setFindQuery(const QString& query)
{
    if (m_findOptions.query == query) {
        return;
    }
    m_findOptions.query = query;
    refreshFind(true);
    emit findChanged();
}

void EditorViewModel::setFindCaseSensitive(bool enabled)
{
    if (m_findOptions.caseSensitive == enabled) {
        return;
    }
    m_findOptions.caseSensitive = enabled;
    refreshFind(true);
    emit findChanged();
}

void EditorViewModel::setFindWholeWord(bool enabled)
{
    if (m_findOptions.wholeWord == enabled) {
        return;
    }
    m_findOptions.wholeWord = enabled;
    refreshFind(true);
    emit findChanged();
}

void EditorViewModel::setFindRegex(bool enabled)
{
    if (m_findOptions.regularExpression == enabled) {
        return;
    }
    m_findOptions.regularExpression = enabled;
    refreshFind(true);
    emit findChanged();
}

void EditorViewModel::refreshFind(bool keepPosition)
{
    if (!m_document) {
        m_find.clear();
        return;
    }

    const editor::Position caret = m_document->cursor().position;
    m_find.search(*m_document, m_findOptions);

    // Typing in the find field should walk forward from where the caret
    // already is, not snap to the top of the file on every keystroke.
    if (keepPosition) {
        m_find.selectNearest(caret);
    }

    // Match highlights are drawn by the line renderer, so a changed result set
    // is a changed view even though the text has not moved.
    ++m_revision;
    emit contentsChanged();
}

void EditorViewModel::revealCurrentMatch()
{
    if (!m_document || m_find.isEmpty()) {
        return;
    }

    const editor::Range match = m_find.current();

    // Selected, not merely scrolled to: the user's next keystroke usually
    // replaces the hit, and selecting it makes that work without a further
    // click.
    m_document->setCursorPosition(match.start);
    m_document->setCursorPosition(match.end, true);

    emit scrollToCursorRequested();
}

void EditorViewModel::findNext()
{
    m_find.next();
    revealCurrentMatch();
    ++m_revision;
    emit contentsChanged();
    emit findChanged();
}

void EditorViewModel::findPrevious()
{
    m_find.previous();
    revealCurrentMatch();
    ++m_revision;
    emit contentsChanged();
    emit findChanged();
}

void EditorViewModel::replaceCurrent(const QString& replacement)
{
    if (!m_document || m_find.isEmpty()) {
        return;
    }

    const editor::Range match = m_find.current();
    m_document->replaceRange(match, replacement);

    // The document moved, so every match after this one is at a different
    // place. Re-running from the caret leaves the user on the next hit, which
    // is what pressing Replace repeatedly should do.
    refreshFind(true);
    revealCurrentMatch();
    emit findChanged();
}

void EditorViewModel::replaceAll(const QString& replacement)
{
    if (!m_document || m_find.isEmpty()) {
        return;
    }

    // Built as one new document and applied as a single edit, so replace-all
    // is one undo step. Doing it match by match would push one step each, and
    // undoing a forty-match replacement would take forty presses.
    //
    // Assembled backwards from the end so each match's recorded position stays
    // valid: replacing forwards would shift everything after the first edit.
    const std::vector<editor::Range> matches = m_find.matches();

    QString text = m_document->text();
    const editor::TextBuffer& buffer = m_document->buffer();

    for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
        const int from = buffer.offsetOf(it->start);
        const int to = buffer.offsetOf(it->end);
        text.replace(from, to - from, replacement);
    }

    m_document->replaceRange(
        editor::Range{editor::Position{0, 0}, buffer.endPosition()}, text);

    refreshFind(false);
    emit findChanged();
}

QVariantList EditorViewModel::matchesOnLine(int line) const
{
    QVariantList result;
    if (!m_findOpen) {
        return result;
    }

    const std::vector<editor::Range>& matches = m_find.matches();
    const int currentIndex = m_find.currentIndex();

    for (size_t i = 0; i < matches.size(); ++i) {
        const editor::Range& match = matches[i];
        if (match.start.line != line) {
            continue;
        }

        // The current match is drawn differently from the rest, so the user can
        // see which one Enter will move away from.
        QVariantMap entry;
        entry.insert(QStringLiteral("start"), match.start.column);
        entry.insert(QStringLiteral("end"), match.end.column);
        entry.insert(QStringLiteral("current"), static_cast<int>(i) == currentIndex);
        result.append(entry);
    }
    return result;
}

void EditorViewModel::toggleLineComment()
{
    if (!m_document) {
        return;
    }

    const QString prefix = m_highlighter.lineCommentPrefix();
    if (prefix.isEmpty()) {
        return;   // a language with no line comment; nothing honest to do
    }

    const editor::Range selection = m_document->cursor().selection();
    const int first = selection.start.line;
    // A selection ending at column 0 stops on the line above: the user dragged
    // to the start of the next line, they did not mean to include it.
    const int last = (!selection.isEmpty() && selection.end.column == 0)
                         ? std::max(first, selection.end.line - 1)
                         : selection.end.line;

    // Commented only if every non-blank line already is. One uncommented line
    // in the block means the intent is to comment.
    bool allCommented = true;
    for (int line = first; line <= last; ++line) {
        const QString text = m_document->line(line);
        if (text.trimmed().isEmpty()) {
            continue;
        }
        if (!text.trimmed().startsWith(prefix)) {
            allCommented = false;
            break;
        }
    }

    // Backwards, so each edit leaves the lines above it at the same numbers.
    for (int line = last; line >= first; --line) {
        const QString text = m_document->line(line);
        if (text.trimmed().isEmpty()) {
            continue;
        }

        const int indent = static_cast<int>(text.size() - text.trimmed().size());
        if (allCommented) {
            // Remove the marker and the single space after it, if it is there -
            // which is what this puts in, so a round trip is lossless.
            int width = prefix.size();
            if (text.mid(indent + width, 1) == QLatin1String(" ")) {
                ++width;
            }
            m_document->replaceRange(
                editor::Range{editor::Position{line, indent},
                              editor::Position{line, indent + width}},
                QString());
        } else {
            m_document->replaceRange(
                editor::Range{editor::Position{line, indent},
                              editor::Position{line, indent}},
                prefix + QLatin1Char(' '));
        }
    }
}

// ---- Bracket matching ------------------------------------------------------

namespace {

/// The partner of a bracket, and which way to look for it. Zero means the
/// character is not a bracket.
QChar partnerOf(QChar character, int& direction)
{
    switch (character.unicode()) {
    case '(': direction = 1;  return QLatin1Char(')');
    case '[': direction = 1;  return QLatin1Char(']');
    case '{': direction = 1;  return QLatin1Char('}');
    case ')': direction = -1; return QLatin1Char('(');
    case ']': direction = -1; return QLatin1Char('[');
    case '}': direction = -1; return QLatin1Char('{');
    default:  direction = 0;  return QChar();
    }
}

/// How far to search before giving up.
///
/// A missing closing brace would otherwise scan to the end of the document on
/// every keystroke. Highlighting is a convenience; spending a frame on it in a
/// file with an unbalanced brace is not a trade worth making.
constexpr int kMaxBracketScanLines = 2000;

} // namespace

void EditorViewModel::updateBracketMatch()
{
    const editor::Position previous = m_bracketMatch;
    const bool wasMatched = m_bracketMatched;

    m_bracket = editor::Position{-1, -1};
    m_bracketMatch = editor::Position{-1, -1};
    m_bracketMatched = false;

    if (!m_document) {
        return;
    }

    const editor::Position caret = m_document->cursor().position;
    const QString line = m_document->line(caret.line);

    // The character at the caret, or the one before it. Both count, which is
    // what makes the highlight appear when you type a closing brace as well as
    // when you arrow onto an opening one.
    int column = -1;
    QChar bracket;
    int direction = 0;

    if (caret.column < line.size()) {
        const QChar at = line.at(caret.column);
        if (!partnerOf(at, direction).isNull()) {
            column = caret.column;
            bracket = at;
        }
    }
    if (column < 0 && caret.column > 0) {
        const QChar before = line.at(caret.column - 1);
        if (!partnerOf(before, direction).isNull()) {
            column = caret.column - 1;
            bracket = before;
        }
    }

    if (column < 0) {
        if (previous.line >= 0 || wasMatched) {
            emit cursorChanged();   // the highlight has to be taken down
        }
        return;
    }

    m_bracket = editor::Position{caret.line, column};

    const QChar partner = partnerOf(bracket, direction);
    int depth = 0;
    int scanned = 0;

    // Nesting is counted, not just the next occurrence: the partner of the
    // outer brace in `{ { } }` is the last one, not the first one found.
    for (int lineIndex = caret.line;
         lineIndex >= 0 && lineIndex < m_document->lineCount()
         && scanned < kMaxBracketScanLines;
         lineIndex += direction, ++scanned) {

        const QString text = m_document->line(lineIndex);
        int from = (lineIndex == caret.line) ? column
                                             : (direction > 0 ? 0 : text.size() - 1);

        for (int i = from; i >= 0 && i < text.size(); i += direction) {
            const QChar character = text.at(i);
            if (character == bracket) {
                ++depth;
            } else if (character == partner) {
                --depth;
                if (depth == 0) {
                    m_bracketMatch = editor::Position{lineIndex, i};
                    m_bracketMatched = true;
                    emit cursorChanged();
                    return;
                }
            }
        }
    }

    // Found a bracket but no partner. Reported as unmatched rather than as
    // nothing, so the view can say so.
    emit cursorChanged();
}

// ---- Additional carets ------------------------------------------------------

int EditorViewModel::cursorCount() const
{
    return m_document ? m_document->cursorCount() : 0;
}

QVariantList EditorViewModel::cursorsOnLine(int line) const
{
    QVariantList result;
    if (!m_document || !m_document->hasMultipleCursors()) {
        return result;   // the ordinary case costs nothing
    }

    for (const editor::Cursor& cursor : m_document->cursors()) {
        if (cursor.position.line == line) {
            result.append(cursor.position.column);
        }
    }
    return result;
}

void EditorViewModel::addCursorAbove()
{
    if (m_document) {
        m_document->addCursorAbove();
    }
}

void EditorViewModel::addCursorBelow()
{
    if (m_document) {
        m_document->addCursorBelow();
    }
}

void EditorViewModel::clearExtraCursors()
{
    if (m_document) {
        m_document->clearExtraCursors();
    }
}

void EditorViewModel::addCursorAt(int line, int column)
{
    if (m_document) {
        m_document->addCursor(editor::Position{line, column});
    }
}

// ---- Folding ---------------------------------------------------------------

int EditorViewModel::visibleLineCount() const
{
    if (!m_document) {
        return 0;
    }
    return m_folds.visibleLineCount(m_document->lineCount());
}

int EditorViewModel::documentLineFor(int visibleRow) const
{
    if (!m_document) {
        return 0;
    }
    return m_folds.documentLineFor(visibleRow, m_document->lineCount());
}

bool EditorViewModel::isFoldable(int line) const
{
    return m_folds.regionAt(line) != nullptr;
}

bool EditorViewModel::isFolded(int line) const
{
    return m_folds.isFolded(line);
}

int EditorViewModel::foldedLineCount(int line) const
{
    const editor::FoldRegion* region = m_folds.regionAt(line);
    return region ? region->endLine - region->startLine : 0;
}

void EditorViewModel::toggleFold(int line)
{
    m_folds.toggle(line);

    // The row count changed, so the view has to rebuild its delegates.
    ++m_revision;
    emit contentsChanged();
}

void EditorViewModel::foldAll()
{
    m_folds.foldAll();
    ++m_revision;
    emit contentsChanged();
}

void EditorViewModel::unfoldAll()
{
    m_folds.unfoldAll();
    ++m_revision;
    emit contentsChanged();
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

namespace {

/// Escapes a run of source text for Text.StyledText, keeping its spacing.
///
/// `toHtmlEscaped` handles the markup characters but not whitespace, and
/// StyledText collapses runs of spaces the way HTML does - so every indented
/// line rendered flush left, and the structure of the code disappeared. Each
/// space becomes a non-breaking space, and a tab becomes four of them.
///
/// Only the runs that matter are converted: a single space between words can
/// stay a plain space, and converting every one would double the size of the
/// markup for no visible difference. `atLineStart` says whether this fragment
/// begins its line - the helper runs once per token, so it cannot tell from
/// what it has built so far, and without it every token's first space would be
/// treated as indentation.
QString escapeForDisplay(const QString& text, bool atLineStart)
{
    QString escaped = text.toHtmlEscaped();

    // Leading whitespace carries the indentation, which is the part that must
    // survive. Runs inside a line - alignment in a table of constants, say -
    // matter too, so any run of two or more is preserved.
    QString result;
    result.reserve(escaped.size());

    int index = 0;
    while (index < escaped.size()) {
        const QChar character = escaped.at(index);

        if (character == QLatin1Char('\t')) {
            result += QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;");
            ++index;
            continue;
        }

        if (character == QLatin1Char(' ')) {
            int run = 0;
            while (index + run < escaped.size()
                   && escaped.at(index + run) == QLatin1Char(' ')) {
                ++run;
            }

            const bool leading = atLineStart && result.isEmpty();
            if (run == 1 && !leading) {
                result += QLatin1Char(' ');
            } else {
                for (int i = 0; i < run; ++i) {
                    result += QStringLiteral("&nbsp;");
                }
            }
            index += run;
            continue;
        }

        result += character;
        ++index;
    }
    return result;
}

} // namespace

QString EditorViewModel::highlightedLine(int line) const
{
    if (!m_document) {
        return QString();
    }

    const QString text = m_document->line(line);
    if (!m_highlighted || text.isEmpty()) {
        return escapeForDisplay(text, true);
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
        return escapeForDisplay(text, true);
    }

    // Built as one string with spans only where a token needs one. Plain runs
    // carry no markup, which keeps the common line short.
    QString html;
    html.reserve(text.size() * 2);

    int position = 0;
    for (const editor::Token& token : tokens) {
        if (token.start > position) {
            html += escapeForDisplay(text.mid(position, token.start - position),
                                     position == 0);
        }

        const QString colour = colourFor(token.kind);
        const QString body =
            escapeForDisplay(text.mid(token.start, token.length), token.start == 0);

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
        html += escapeForDisplay(text.mid(position), position == 0);
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

    // A closing brace typed as the first thing on a line pulls that line back
    // one level, so `}` lands under the `{` it closes rather than one level in
    // from it. Only when nothing but whitespace precedes it: a brace at the end
    // of an expression is not closing a block.
    if (text.size() == 1
        && (text == QLatin1String("}") || text == QLatin1String(")")
            || text == QLatin1String("]"))) {
        const editor::Position caret = m_document->cursor().position;
        const QString line = m_document->line(caret.line);
        const QString before = line.left(caret.column);

        if (!before.isEmpty() && before.trimmed().isEmpty()) {
            const QString unit = m_settings.indentString();
            if (before.endsWith(unit)) {
                m_document->replaceRange(
                    editor::Range{
                        editor::Position{caret.line,
                                         caret.column - static_cast<int>(unit.size())},
                        editor::Position{caret.line, caret.column}},
                    QString());
            }
        }
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
    QString lead = current.left(carry);

    // One level deeper after a line that opens a block. Every editor does this,
    // and without it the first thing anyone does after each brace is press Tab.
    const int caretColumn = m_document->cursor().position.column;
    const QString before = current.left(caretColumn).trimmed();
    if (before.endsWith(QLatin1Char('{')) || before.endsWith(QLatin1Char('('))
        || before.endsWith(QLatin1Char('[')) || before.endsWith(QLatin1Char(':'))) {
        lead += m_settings.indentString();
    }

    m_document->insertText(QLatin1String("\n") + lead);
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
