#pragma once

#include "editor/TextBuffer.h"

#include <QString>

#include <vector>

namespace keys::editor {

/// One reversible change to the document.
///
/// Stored as the text removed and the text inserted over a range, which is
/// enough to apply the edit forwards or backwards without consulting the buffer.
struct Edit {
    Range range;            ///< what the edit replaced, before it was applied
    QString removedText;    ///< what was there
    QString insertedText;   ///< what replaced it

    /// Where the caret was before and after, so undo restores the view as well
    /// as the text. An undo that fixes the characters but leaves the caret
    /// elsewhere makes the user hunt for their place.
    Position cursorBefore;
    Position cursorAfter;

    /// True when this edit continues the one below it in the stack, so undo
    /// takes both back together.
    ///
    /// Coalescing handles a run of typing, but not a single action that
    /// deliberately makes several edits at once - typing one character at four
    /// carets is one thing the user did, and four presses of Ctrl+Z to take it
    /// back would be absurd. Coalescing cannot express that: the edits are at
    /// different places and would never merge.
    bool continuesGroup = false;
};

/// Undo and redo for a document.
///
/// **Coalescing.** Typing one character at a time would otherwise produce one
/// undo step per keystroke, so undoing a sentence would take a hundred presses.
/// Consecutive single-character insertions that continue at the caret are merged
/// into one step, and the merge is broken by anything that suggests a new
/// thought: a newline, a caret jump, a deletion, a save, or a pause.
///
/// This is the behaviour every mature editor converges on, because per-keystroke
/// undo is unusable and per-line undo loses too much.
class UndoStack {
public:
    /// Records an edit. May merge it into the previous one; see coalescing above.
    void push(const Edit& edit);

    [[nodiscard]] bool canUndo() const { return m_index > 0; }
    [[nodiscard]] bool canRedo() const { return m_index < static_cast<int>(m_edits.size()); }

    /// The edit to reverse, or nullptr if there is nothing to undo. The caller
    /// applies it; the stack only tracks position.
    [[nodiscard]] const Edit* undo();

    /// The edit to reapply, or nullptr if there is nothing to redo.
    [[nodiscard]] const Edit* redo();

    /// Whether the next undo or redo belongs to the same group as the one just
    /// returned. The document loops on these so a grouped action is taken back
    /// in one press.
    [[nodiscard]] bool undoContinues() const;
    [[nodiscard]] bool redoContinues() const;

    /// Ends the current coalescing run, so the next edit starts a fresh step.
    /// Called on save, on a caret move, and when the document loses focus.
    void breakMergePoint() { m_canMerge = false; }

    /// Opens and closes a group. Every edit pushed between the two is undone
    /// and redone as one step, however far apart in the document they are.
    void beginGroup();
    void endGroup();

    void clear();

    [[nodiscard]] int count() const { return static_cast<int>(m_edits.size()); }

    /// How many edits back the last save was, or -1 if the saved state is no
    /// longer reachable (the redo branch it lived on was discarded). Lets the
    /// document report itself clean again when the user undoes back to a save.
    [[nodiscard]] bool isAtSavePoint() const { return m_index == m_savePoint; }
    void markSavePoint();

private:
    /// True if `edit` continues the run at the top of the stack.
    [[nodiscard]] bool shouldMerge(const Edit& edit) const;

    std::vector<Edit> m_edits;

    /// How many edits are currently applied. Undo moves it back, redo forward,
    /// and a new edit truncates everything above it.
    int m_index = 0;

    /// The value of m_index when the document was last saved. -1 once that
    /// state becomes unreachable.
    int m_savePoint = 0;

    /// Whether the next insertion may join the previous one.
    bool m_canMerge = false;

    /// Inside beginGroup()/endGroup(). The first edit of a group starts a new
    /// step; every later one is marked as continuing it.
    bool m_grouping = false;
    bool m_groupStarted = false;
};

} // namespace keys::editor
