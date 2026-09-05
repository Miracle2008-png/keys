#pragma once

#include "core/Result.h"
#include "editor/TextBuffer.h"
#include "editor/UndoStack.h"

#include <QObject>
#include <QString>

namespace keys::editor {

/// A caret and its selection.
///
/// `anchor` is where the selection began, `position` is where the caret is. They
/// are equal when there is no selection. Keeping the anchor separate is what
/// makes shift-extension work in both directions from a fixed point.
struct Cursor {
    Position position;
    Position anchor;

    /// The column the caret "wants" when moving vertically. Moving down from a
    /// long line through a short one and back must return to the original
    /// column - without this, the caret walks left every time it passes a short
    /// line. -1 means unset.
    int desiredColumn = -1;

    [[nodiscard]] bool hasSelection() const { return position != anchor; }
    [[nodiscard]] Range selection() const { return Range{anchor, position}.normalized(); }
};

/// One open document: its text, caret, selection, undo history and dirty state.
///
/// This is the editor's model. It has no idea how text is drawn or measured -
/// rendering lives in the UI layer and talks to this through positions and
/// ranges. That separation is what lets the whole editing model be tested
/// without a scene graph.
///
/// Milestone 4 supports one cursor. The type is shaped for many (Cursor is a
/// value, and edits are expressed as ranges) so multi-cursor is an extension
/// rather than a rewrite.
class TextDocument : public QObject {
    Q_OBJECT

public:
    explicit TextDocument(QObject* parent = nullptr);

    // ---- Content ----------------------------------------------------------

    /// Replaces the contents and clears the history, as when loading a file.
    /// The document is clean afterwards.
    void setText(const QString& text);

    [[nodiscard]] QString text() const { return m_buffer.text(); }
    [[nodiscard]] QString line(int index) const { return m_buffer.line(index); }
    [[nodiscard]] int lineCount() const { return m_buffer.lineCount(); }
    [[nodiscard]] int lineLength(int index) const { return m_buffer.lineLength(index); }
    [[nodiscard]] const TextBuffer& buffer() const { return m_buffer; }

    /// Absolute path this document was loaded from, empty for an unsaved one.
    [[nodiscard]] const QString& path() const { return m_path; }
    void setPath(const QString& path);

    /// True when the text differs from what was last saved.
    [[nodiscard]] bool isModified() const { return m_modified; }

    // ---- Editing ----------------------------------------------------------

    /// Replaces the current selection (or inserts at the caret) with `text`.
    void insertText(const QString& text);

    /// Deletes the selection, or one character before the caret if there is
    /// none - the Backspace behaviour.
    void deleteBackward();

    /// Deletes the selection, or one character after the caret - Delete.
    void deleteForward();

    /// Removes an explicit range, leaving the caret at its start.
    void removeRange(const Range& range);

    bool undo();
    bool redo();

    [[nodiscard]] bool canUndo() const { return m_undo.canUndo(); }
    [[nodiscard]] bool canRedo() const { return m_undo.canRedo(); }

    /// Marks the current state as saved.
    void markSaved();

    // ---- Cursor and selection ---------------------------------------------

    [[nodiscard]] const Cursor& cursor() const { return m_cursor; }

    /// Moves the caret, collapsing any selection unless `extend` is true.
    void setCursorPosition(const Position& position, bool extend = false);

    void moveLeft(bool extend = false);
    void moveRight(bool extend = false);
    void moveUp(bool extend = false);
    void moveDown(bool extend = false);
    void moveToLineStart(bool extend = false);
    void moveToLineEnd(bool extend = false);
    void moveToDocumentStart(bool extend = false);
    void moveToDocumentEnd(bool extend = false);

    /// Word-wise movement, for Ctrl+Left and Ctrl+Right.
    void moveWordLeft(bool extend = false);
    void moveWordRight(bool extend = false);

    void selectAll();
    void selectLine(int line);

    [[nodiscard]] QString selectedText() const;

signals:
    /// The text changed. Carries the range that was replaced and how many lines
    /// the document gained or lost, so a view can update incrementally rather
    /// than re-laying out everything.
    void contentsChanged(const Range& replacedRange, int lineCountDelta);

    void cursorChanged();
    void modifiedChanged(bool modified);
    void undoAvailabilityChanged();

    /// The document now represents a different file. The view binds to the
    /// path, so without this it would keep showing the previous one.
    void pathChanged(const QString& path);

private:
    /// Applies an edit and records it for undo. The single funnel every mutation
    /// goes through, so nothing can change the text without becoming undoable.
    void applyEdit(const Range& range, const QString& replacement);

    void setModified(bool modified);

    /// Moves the caret and emits, collapsing the selection unless extending.
    void moveCursorTo(const Position& position, bool extend, bool keepDesiredColumn = false);

    /// The next word boundary in each direction, used by moveWord*.
    [[nodiscard]] Position wordBoundaryLeft(const Position& from) const;
    [[nodiscard]] Position wordBoundaryRight(const Position& from) const;

    TextBuffer m_buffer;
    UndoStack m_undo;
    Cursor m_cursor;

    QString m_path;
    bool m_modified = false;

    /// Guards against an undo's own edits being pushed back onto the stack.
    bool m_applyingHistory = false;
};

} // namespace keys::editor
