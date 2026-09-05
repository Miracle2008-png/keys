#include "editor/UndoStack.h"

namespace keys::editor {

bool UndoStack::shouldMerge(const Edit& edit) const
{
    if (!m_canMerge || m_index == 0 || m_index != static_cast<int>(m_edits.size())) {
        return false;
    }

    const Edit& previous = m_edits.at(static_cast<size_t>(m_index) - 1);

    // Only pure insertions merge. A deletion is a distinct intent, and merging
    // one into a run of typing would make a single undo remove text the user
    // never typed.
    if (!edit.removedText.isEmpty() || !previous.removedText.isEmpty()) {
        return false;
    }

    // Typing a newline ends the thought. Undoing back through several lines at
    // once is more surprising than useful.
    if (edit.insertedText.contains(QLatin1Char('\n'))
        || previous.insertedText.contains(QLatin1Char('\n'))) {
        return false;
    }

    // Only single characters merge: a paste is one deliberate action and should
    // undo as one, not join whatever was being typed before it.
    if (edit.insertedText.size() != 1) {
        return false;
    }

    // The new text must continue exactly where the previous run ended. A caret
    // moved elsewhere means the user is editing somewhere else.
    return edit.range.start == previous.cursorAfter;
}

void UndoStack::push(const Edit& edit)
{
    if (shouldMerge(edit)) {
        Edit& previous = m_edits.at(static_cast<size_t>(m_index) - 1);
        previous.insertedText += edit.insertedText;
        previous.cursorAfter = edit.cursorAfter;
        return;
    }

    // A new edit after undoing discards the redo branch: the future the user
    // undid away is no longer reachable.
    if (m_index < static_cast<int>(m_edits.size())) {
        m_edits.erase(m_edits.begin() + m_index, m_edits.end());

        // If the save point lived on the discarded branch, the saved state can
        // no longer be reached by undoing, so the document can never report
        // itself clean again until it is saved anew.
        if (m_savePoint > m_index) {
            m_savePoint = -1;
        }
    }

    m_edits.push_back(edit);
    ++m_index;

    // Only a single-character insertion can start a mergeable run.
    m_canMerge = edit.removedText.isEmpty()
                 && edit.insertedText.size() == 1
                 && !edit.insertedText.contains(QLatin1Char('\n'));
}

const Edit* UndoStack::undo()
{
    if (!canUndo()) {
        return nullptr;
    }

    // Undoing always ends the run: typing after an undo starts a new step.
    m_canMerge = false;

    --m_index;
    return &m_edits.at(static_cast<size_t>(m_index));
}

const Edit* UndoStack::redo()
{
    if (!canRedo()) {
        return nullptr;
    }

    m_canMerge = false;

    const Edit* edit = &m_edits.at(static_cast<size_t>(m_index));
    ++m_index;
    return edit;
}

void UndoStack::clear()
{
    m_edits.clear();
    m_index = 0;
    m_savePoint = 0;
    m_canMerge = false;
}

void UndoStack::markSavePoint()
{
    m_savePoint = m_index;

    // Saving ends the run: text typed after a save is a separate step from text
    // typed before it.
    m_canMerge = false;
}

} // namespace keys::editor
