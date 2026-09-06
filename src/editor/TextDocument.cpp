#include "editor/TextDocument.h"

#include <algorithm>

#include <QChar>

namespace keys::editor {
namespace {

/// What kind of run a character belongs to, for word-wise movement.
///
/// Three classes rather than word/non-word, so Ctrl+Right stops between
/// `foo` and `(` in `foo(bar)` but treats a run of punctuation as one unit.
enum class CharClass { Whitespace, Word, Punctuation };

CharClass classify(QChar character)
{
    if (character.isSpace()) {
        return CharClass::Whitespace;
    }
    // Underscore counts as a word character: `snake_case` is one identifier to
    // a programmer, and stopping inside it would be wrong in every language
    // Keys is likely to open.
    if (character.isLetterOrNumber() || character == QLatin1Char('_')) {
        return CharClass::Word;
    }
    return CharClass::Punctuation;
}

} // namespace

TextDocument::TextDocument(QObject* parent) : QObject(parent) {}

void TextDocument::setText(const QString& text)
{
    const int previousLines = m_buffer.lineCount();

    m_buffer.setText(text);
    m_undo.clear();
    m_cursor = Cursor{};
    m_extraCursors.clear();

    setModified(false);

    emit contentsChanged(Range{{0, 0}, m_buffer.endPosition()},
                         m_buffer.lineCount() - previousLines);
    emit cursorChanged();
    emit undoAvailabilityChanged();
}

void TextDocument::setPath(const QString& path)
{
    if (m_path == path) {
        return;
    }
    m_path = path;
    emit pathChanged(m_path);
}

void TextDocument::setModified(bool modified)
{
    if (m_modified == modified) {
        return;
    }
    m_modified = modified;
    emit modifiedChanged(m_modified);
}

void TextDocument::markSaved()
{
    m_undo.markSavePoint();
    setModified(false);
}

// ---- Additional carets ------------------------------------------------------

std::vector<Cursor> TextDocument::cursors() const
{
    std::vector<Cursor> all;
    all.reserve(m_extraCursors.size() + 1);
    all.push_back(m_cursor);
    all.insert(all.end(), m_extraCursors.begin(), m_extraCursors.end());

    std::sort(all.begin(), all.end(), [](const Cursor& a, const Cursor& b) {
        return a.position < b.position;
    });
    return all;
}

void TextDocument::addCursor(const Position& position)
{
    const Position clamped = m_buffer.clamp(position);

    // Two carets in one place would type every character twice.
    if (clamped == m_cursor.position) {
        return;
    }
    for (const Cursor& existing : m_extraCursors) {
        if (existing.position == clamped) {
            return;
        }
    }

    Cursor cursor;
    cursor.position = clamped;
    cursor.anchor = clamped;
    m_extraCursors.push_back(cursor);

    emit cursorChanged();
}

void TextDocument::clearExtraCursors()
{
    if (m_extraCursors.empty()) {
        return;
    }
    m_extraCursors.clear();
    emit cursorChanged();
}

void TextDocument::addCursorBelow()
{
    // From the lowest caret, so repeating the shortcut walks down the file
    // rather than adding the same caret again.
    Position lowest = m_cursor.position;
    for (const Cursor& cursor : m_extraCursors) {
        if (lowest < cursor.position) {
            lowest = cursor.position;
        }
    }
    if (lowest.line + 1 < m_buffer.lineCount()) {
        addCursor(Position{lowest.line + 1, lowest.column});
    }
}

void TextDocument::addCursorAbove()
{
    Position highest = m_cursor.position;
    for (const Cursor& cursor : m_extraCursors) {
        if (cursor.position < highest) {
            highest = cursor.position;
        }
    }
    if (highest.line > 0) {
        addCursor(Position{highest.line - 1, highest.column});
    }
}

void TextDocument::forEachCursorReversed(const std::function<void(Cursor&)>& edit)
{
    if (m_extraCursors.empty()) {
        edit(m_cursor);
        return;
    }

    // Every caret in one list, applied from the bottom of the document upwards
    // so that each edit happens at a position the ones before it have not moved.
    std::vector<Cursor*> ordered;
    ordered.reserve(m_extraCursors.size() + 1);
    ordered.push_back(&m_cursor);
    for (Cursor& cursor : m_extraCursors) {
        ordered.push_back(&cursor);
    }

    std::sort(ordered.begin(), ordered.end(), [](const Cursor* a, const Cursor* b) {
        return b->position < a->position;
    });

    // One undo step for the whole thing: typing a character at four carets is
    // one action, and taking it back should be one press.
    m_undo.breakMergePoint();
    for (Cursor* cursor : ordered) {
        edit(*cursor);
    }
}

void TextDocument::applyAtEveryCursor(
    const std::function<Range(const Cursor&)>& rangeFor, const QString& replacement)
{
    // Bottom-up, so each edit lands at a position the ones before it have not
    // shifted. Going forwards, the second caret would already be stale.
    std::vector<Cursor> ordered = cursors();
    std::sort(ordered.begin(), ordered.end(), [](const Cursor& a, const Cursor& b) {
        return b.position < a.position;
    });

    // One undo step for the lot: typing a character at four carets is one
    // action, and taking it back should be one press of Ctrl+Z.
    m_undo.beginGroup();

    std::vector<Position> resulting;
    resulting.reserve(ordered.size());

    for (const Cursor& cursor : ordered) {
        const Range target = rangeFor(cursor);
        if (target.isEmpty() && replacement.isEmpty()) {
            resulting.push_back(cursor.position);
            continue;
        }
        applyEdit(target, replacement);
        resulting.push_back(m_cursor.position);
    }

    m_undo.endGroup();

    // applyEdit leaves m_cursor wherever the last edit finished; rebuild the
    // full set from where each one actually ended up.
    std::sort(resulting.begin(), resulting.end());

    m_extraCursors.clear();
    m_cursor.position = resulting.front();
    m_cursor.anchor = resulting.front();
    m_cursor.desiredColumn = -1;

    for (size_t i = 1; i < resulting.size(); ++i) {
        Cursor extra;
        extra.position = resulting[i];
        extra.anchor = resulting[i];
        m_extraCursors.push_back(extra);
    }

    emit cursorChanged();
}

void TextDocument::applyEdit(const Range& range, const QString& replacement)
{
    const Range ordered = range.normalized();

    Edit edit;
    edit.range = ordered;
    edit.removedText = m_buffer.textIn(ordered);
    edit.insertedText = replacement;
    edit.cursorBefore = m_cursor.position;

    const int linesBefore = m_buffer.lineCount();

    Position caret = m_buffer.remove(ordered);
    if (!replacement.isEmpty()) {
        caret = m_buffer.insert(caret, replacement);
    }

    edit.cursorAfter = caret;

    // An undo's own edits must not be recorded, or undo would push a new step
    // and the history would grow every time the user pressed Ctrl+Z.
    if (!m_applyingHistory) {
        m_undo.push(edit);
        emit undoAvailabilityChanged();
    }

    m_cursor.position = caret;
    m_cursor.anchor = caret;
    m_cursor.desiredColumn = -1;

    setModified(!m_undo.isAtSavePoint());

    emit contentsChanged(ordered, m_buffer.lineCount() - linesBefore);
    emit cursorChanged();
}

void TextDocument::insertText(const QString& text)
{
    if (m_extraCursors.empty()) {
        if (text.isEmpty() && !m_cursor.hasSelection()) {
            return;
        }
        const Range target = m_cursor.hasSelection()
                                 ? m_cursor.selection()
                                 : Range{m_cursor.position, m_cursor.position};
        applyEdit(target, text);
        return;
    }
    applyAtEveryCursor([&](const Cursor& cursor) {
        return cursor.hasSelection() ? cursor.selection()
                                     : Range{cursor.position, cursor.position};
    }, text);
}

void TextDocument::deleteBackward()
{
    if (!m_extraCursors.empty()) {
        applyAtEveryCursor([this](const Cursor& cursor) {
            if (cursor.hasSelection()) {
                return cursor.selection();
            }
            if (cursor.position.line == 0 && cursor.position.column == 0) {
                return Range{cursor.position, cursor.position};  // nothing before
            }
            const Position previous =
                m_buffer.positionOf(m_buffer.offsetOf(cursor.position) - 1);
            return Range{previous, cursor.position};
        }, QString());
        return;
    }

    if (m_cursor.hasSelection()) {
        applyEdit(m_cursor.selection(), QString());
        return;
    }

    const Position caret = m_cursor.position;
    if (caret.line == 0 && caret.column == 0) {
        return;   // nothing before the start of the document
    }

    // Stepping back through the offset rather than decrementing the column
    // handles the line boundary without a special case.
    const Position previous = m_buffer.positionOf(m_buffer.offsetOf(caret) - 1);
    applyEdit(Range{previous, caret}, QString());
}

void TextDocument::deleteForward()
{
    if (!m_extraCursors.empty()) {
        applyAtEveryCursor([this](const Cursor& cursor) {
            if (cursor.hasSelection()) {
                return cursor.selection();
            }
            if (cursor.position == m_buffer.endPosition()) {
                return Range{cursor.position, cursor.position};
            }
            const Position next =
                m_buffer.positionOf(m_buffer.offsetOf(cursor.position) + 1);
            return Range{cursor.position, next};
        }, QString());
        return;
    }

    if (m_cursor.hasSelection()) {
        applyEdit(m_cursor.selection(), QString());
        return;
    }

    const Position caret = m_cursor.position;
    if (caret == m_buffer.endPosition()) {
        return;
    }

    const Position next = m_buffer.positionOf(m_buffer.offsetOf(caret) + 1);
    applyEdit(Range{caret, next}, QString());
}

void TextDocument::removeRange(const Range& range)
{
    const Range ordered = range.normalized();
    if (ordered.isEmpty()) {
        return;
    }
    applyEdit(ordered, QString());
}

void TextDocument::replaceRange(const Range& range, const QString& replacement)
{
    const Range ordered = range.normalized();
    if (ordered.isEmpty() && replacement.isEmpty()) {
        return;
    }

    // Ends any run of coalesced typing: a replacement is a separate intent from
    // whatever was being typed before it, and merging the two would make one
    // undo take back both.
    m_undo.breakMergePoint();
    applyEdit(ordered, replacement);
}

bool TextDocument::undo()
{
    const Edit* edit = m_undo.undo();
    if (!edit) {
        return false;
    }

    Position restoreTo;

    // A grouped action - one keystroke applied at several carets, or a
    // replace-all - is several edits that the user made as one thing, so undo
    // keeps going until the group is exhausted.
    for (;;) {
        // Reverse it: put back what was removed, over what was inserted.
        const Position start = edit->range.start;
        const Position insertedEnd =
            m_buffer.positionOf(m_buffer.offsetOf(start) + edit->insertedText.size());

        m_applyingHistory = true;
        applyEdit(Range{start, insertedEnd}, edit->removedText);
        m_applyingHistory = false;

        restoreTo = edit->cursorBefore;

        if (!m_undo.undoContinues()) {
            break;
        }
        edit = m_undo.undo();
        if (!edit) {
            break;
        }
    }

    // Restore the caret to where it was before the edit, so undo returns the
    // user to their place rather than to wherever the text happens to end.
    m_extraCursors.clear();
    m_cursor.position = m_buffer.clamp(restoreTo);
    m_cursor.anchor = m_cursor.position;

    setModified(!m_undo.isAtSavePoint());

    emit cursorChanged();
    emit undoAvailabilityChanged();
    return true;
}

bool TextDocument::redo()
{
    const Edit* edit = m_undo.redo();
    if (!edit) {
        return false;
    }

    const Position start = edit->range.start;
    const Position removedEnd =
        m_buffer.positionOf(m_buffer.offsetOf(start) + edit->removedText.size());

    m_applyingHistory = true;
    applyEdit(Range{start, removedEnd}, edit->insertedText);
    m_applyingHistory = false;

    m_cursor.position = m_buffer.clamp(edit->cursorAfter);
    m_cursor.anchor = m_cursor.position;

    setModified(!m_undo.isAtSavePoint());

    emit cursorChanged();
    emit undoAvailabilityChanged();
    return true;
}

// ---- Cursor movement --------------------------------------------------------

void TextDocument::moveCursorTo(const Position& position, bool extend,
                                bool keepDesiredColumn)
{
    const Position clamped = m_buffer.clamp(position);

    m_cursor.position = clamped;
    if (!extend) {
        m_cursor.anchor = clamped;
    }

    // Vertical movement preserves the desired column; everything else resets it,
    // because the user has expressed a new horizontal intent.
    if (!keepDesiredColumn) {
        m_cursor.desiredColumn = -1;
    }

    // A caret move ends the typing run: text typed after moving is a separate
    // undo step from text typed before.
    m_undo.breakMergePoint();

    emit cursorChanged();
}

void TextDocument::setCursorPosition(const Position& position, bool extend)
{
    moveCursorTo(position, extend);
}

void TextDocument::moveLeft(bool extend)
{
    // With a selection and no extension, the caret collapses to its start
    // rather than moving one further - the behaviour every editor uses.
    if (m_cursor.hasSelection() && !extend) {
        moveCursorTo(m_cursor.selection().start, false);
        return;
    }

    const int offset = m_buffer.offsetOf(m_cursor.position);
    if (offset > 0) {
        moveCursorTo(m_buffer.positionOf(offset - 1), extend);
    }
}

void TextDocument::moveRight(bool extend)
{
    if (m_cursor.hasSelection() && !extend) {
        moveCursorTo(m_cursor.selection().end, false);
        return;
    }

    const int offset = m_buffer.offsetOf(m_cursor.position);
    if (offset < m_buffer.length()) {
        moveCursorTo(m_buffer.positionOf(offset + 1), extend);
    }
}

void TextDocument::moveUp(bool extend)
{
    if (m_cursor.position.line == 0) {
        // Already on the first line: go to its start, matching every editor.
        moveCursorTo(Position{0, 0}, extend);
        return;
    }

    // Remember the column being aimed for, so passing through a short line does
    // not drag the caret permanently left.
    if (m_cursor.desiredColumn < 0) {
        m_cursor.desiredColumn = m_cursor.position.column;
    }

    const int line = m_cursor.position.line - 1;
    moveCursorTo(Position{line, m_cursor.desiredColumn}, extend, true);
}

void TextDocument::moveDown(bool extend)
{
    if (m_cursor.position.line >= m_buffer.lineCount() - 1) {
        moveCursorTo(m_buffer.endPosition(), extend);
        return;
    }

    if (m_cursor.desiredColumn < 0) {
        m_cursor.desiredColumn = m_cursor.position.column;
    }

    const int line = m_cursor.position.line + 1;
    moveCursorTo(Position{line, m_cursor.desiredColumn}, extend, true);
}

void TextDocument::moveToLineStart(bool extend)
{
    const QString text = m_buffer.line(m_cursor.position.line);

    // Home goes to the first non-blank character, and to column 0 only if the
    // caret is already there. On indented code that is almost always what the
    // user means.
    int firstNonBlank = 0;
    while (firstNonBlank < text.size() && text.at(firstNonBlank).isSpace()) {
        ++firstNonBlank;
    }

    const int target = m_cursor.position.column == firstNonBlank ? 0 : firstNonBlank;
    moveCursorTo(Position{m_cursor.position.line, target}, extend);
}

void TextDocument::moveToLineEnd(bool extend)
{
    const int line = m_cursor.position.line;
    moveCursorTo(Position{line, m_buffer.lineLength(line)}, extend);
}

void TextDocument::moveToDocumentStart(bool extend)
{
    moveCursorTo(Position{0, 0}, extend);
}

void TextDocument::moveToDocumentEnd(bool extend)
{
    moveCursorTo(m_buffer.endPosition(), extend);
}

Position TextDocument::wordBoundaryLeft(const Position& from) const
{
    // Word movement scans within the current line, not the whole document.
    // Materialising the entire text for every Ctrl+Left would make a keystroke
    // cost O(document size) - unacceptable on a large file, and unnecessary:
    // a word never spans a line break.
    Position caret = m_buffer.clamp(from);

    // At the start of a line, one press moves to the end of the previous one.
    if (caret.column == 0) {
        if (caret.line == 0) {
            return caret;
        }
        const int previous = caret.line - 1;
        return Position{previous, m_buffer.lineLength(previous)};
    }

    const QString text = m_buffer.line(caret.line);
    int column = caret.column - 1;

    // Skip whitespace behind the caret, then consume the run of like
    // characters. That is what makes one press cross the gap and land at the
    // start of the previous word rather than at its end.
    while (column > 0 && classify(text.at(column)) == CharClass::Whitespace) {
        --column;
    }

    const CharClass run = classify(text.at(column));
    while (column > 0 && classify(text.at(column - 1)) == run) {
        --column;
    }

    return Position{caret.line, column};
}

Position TextDocument::wordBoundaryRight(const Position& from) const
{
    const Position caret = m_buffer.clamp(from);
    const QString text = m_buffer.line(caret.line);

    // At the end of a line, one press moves to the start of the next.
    if (caret.column >= text.size()) {
        if (caret.line + 1 >= m_buffer.lineCount()) {
            return caret;
        }
        return Position{caret.line + 1, 0};
    }

    int column = caret.column;

    // Consume the current run, then any whitespace after it, so the caret lands
    // at the start of the next word.
    const CharClass run = classify(text.at(column));
    while (column < text.size() && classify(text.at(column)) == run) {
        ++column;
    }
    while (column < text.size() && classify(text.at(column)) == CharClass::Whitespace) {
        ++column;
    }

    return Position{caret.line, column};
}

void TextDocument::moveWordLeft(bool extend)
{
    moveCursorTo(wordBoundaryLeft(m_cursor.position), extend);
}

void TextDocument::moveWordRight(bool extend)
{
    moveCursorTo(wordBoundaryRight(m_cursor.position), extend);
}

void TextDocument::selectAll()
{
    m_cursor.anchor = Position{0, 0};
    m_cursor.position = m_buffer.endPosition();
    m_cursor.desiredColumn = -1;

    m_undo.breakMergePoint();
    emit cursorChanged();
}

void TextDocument::selectLine(int line)
{
    if (line < 0 || line >= m_buffer.lineCount()) {
        return;
    }

    m_cursor.anchor = Position{line, 0};

    // Include the terminator so deleting the selection removes the whole line
    // rather than leaving a blank one behind. The last line has none.
    m_cursor.position = line + 1 < m_buffer.lineCount()
                            ? Position{line + 1, 0}
                            : Position{line, m_buffer.lineLength(line)};

    m_cursor.desiredColumn = -1;
    m_undo.breakMergePoint();
    emit cursorChanged();
}

QString TextDocument::selectedText() const
{
    return m_cursor.hasSelection() ? m_buffer.textIn(m_cursor.selection()) : QString();
}

} // namespace keys::editor
