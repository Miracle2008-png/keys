#pragma once

#include "core/Result.h"
#include "editor/TextBuffer.h"
#include "editor/UndoStack.h"

#include <QObject>
#include <QString>

#include <functional>
#include <vector>

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

    /// Replaces an explicit range with `replacement`, as one undoable edit.
    ///
    /// One edit rather than a remove followed by an insert: replace-all covers
    /// a whole document in a single action as far as the user is concerned, and
    /// undoing it should take one press of Ctrl+Z rather than one per match.
    void replaceRange(const Range& range, const QString& replacement);

    bool undo();
    bool redo();

    [[nodiscard]] bool canUndo() const { return m_undo.canUndo(); }
    [[nodiscard]] bool canRedo() const { return m_undo.canRedo(); }

    /// Marks the current state as saved.
    void markSaved();

    // ---- Cursor and selection ---------------------------------------------

    [[nodiscard]] const Cursor& cursor() const { return m_cursor; }

    // ---- Additional carets -------------------------------------------------
    //
    // The primary caret stays `m_cursor` and every existing operation keeps
    // working through it untouched. Extra carets live beside it rather than
    // replacing it with a list, because a rewrite of all 50-odd uses would put
    // the whole editor at risk to add one feature.
    //
    // Edits are applied per caret, from the last to the first, so each one is
    // made at a position the earlier edits have not yet shifted.

    /// Every caret, primary first. One entry when multi-cursor is not in use,
    /// which is the case the editor spends almost all its time in.
    [[nodiscard]] std::vector<Cursor> cursors() const;

    [[nodiscard]] int cursorCount() const
    {
        return 1 + static_cast<int>(m_extraCursors.size());
    }

    [[nodiscard]] bool hasMultipleCursors() const { return !m_extraCursors.empty(); }

    /// Adds a caret. Ignored if one is already there - two carets in the same
    /// place would double every character typed.
    void addCursor(const Position& position);

    /// Drops every caret but the primary. What Escape does, and what any
    /// ordinary click does.
    void clearExtraCursors();

    /// Adds a caret one line above or below the lowest or highest existing one,
    /// at the same column. The usual Ctrl+Alt+Up/Down.
    void addCursorAbove();
    void addCursorBelow();

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

    /// Applies one replacement at every caret, bottom-up, as a single undo
    /// step. `rangeFor` says what each caret replaces - its selection when it
    /// has one, or the empty range at its position.
    void applyAtEveryCursor(const std::function<Range(const Cursor&)>& rangeFor,
                            const QString& replacement);

    /// Runs `edit` at every caret, last to first, as one undo step.
    ///
    /// Backwards because an edit at an earlier position shifts everything after
    /// it: going forwards, the second caret would already be pointing at the
    /// wrong character.
    void forEachCursorReversed(const std::function<void(Cursor&)>& edit);

    TextBuffer m_buffer;
    UndoStack m_undo;
    Cursor m_cursor;

    /// Carets beyond the primary, in document order.
    std::vector<Cursor> m_extraCursors;

    QString m_path;
    bool m_modified = false;

    /// Guards against an undo's own edits being pushed back onto the stack.
    bool m_applyingHistory = false;
};

} // namespace keys::editor
