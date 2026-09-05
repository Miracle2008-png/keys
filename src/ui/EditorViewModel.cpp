#include "ui/EditorViewModel.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>

using keys::editor::Position;
using keys::editor::Range;

namespace keys::ui {

EditorViewModel::EditorViewModel(QObject* parent) : QObject(parent) {}

void EditorViewModel::setDocument(editor::TextDocument* document)
{
    if (m_document == document) {
        return;
    }

    if (m_document) {
        m_document->disconnect(this);
    }

    m_document = document;

    if (m_document) {
        connect(m_document, &editor::TextDocument::contentsChanged,
                this, &EditorViewModel::contentsChanged);
        connect(m_document, &editor::TextDocument::cursorChanged,
                this, &EditorViewModel::cursorChanged);
        connect(m_document, &editor::TextDocument::modifiedChanged,
                this, &EditorViewModel::modifiedChanged);
        connect(m_document, &editor::TextDocument::pathChanged,
                this, &EditorViewModel::documentChanged);
    }

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
    // Spaces rather than a tab character, matching the editor.insertSpaces
    // default. Milestone 9 wires this to the real setting.
    m_document->insertText(QStringLiteral("    "));
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
