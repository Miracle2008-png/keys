#pragma once

#include "core/Result.h"
#include "workspace/EditorGroup.h"

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

namespace keys::workspace {

/// The arrangement of editor panes.
///
/// Milestone 5 supports side-by-side groups — the design's split view. The shape
/// is a list rather than a single `split` flag so a third pane, or a vertical
/// arrangement, is an extension rather than a rewrite. What it deliberately is
/// not is a general nested tree: an arbitrary grid of panes is a large amount of
/// layout machinery for something almost nobody uses, and it can be added later
/// by making a group's slot hold either a group or a nested layout.
///
/// One group is always active. Every command that acts on "the editor" — open,
/// save, close — acts on the active group, so the user's focus determines the
/// target rather than a separate selection the UI has to keep in sync.
class EditorLayout : public QObject {
    Q_OBJECT

public:
    explicit EditorLayout(QObject* parent = nullptr);
    ~EditorLayout() override;

    [[nodiscard]] int groupCount() const { return static_cast<int>(m_groups.size()); }

    [[nodiscard]] EditorGroup* groupAt(int index) const;
    [[nodiscard]] EditorGroup* activeGroup() const;
    [[nodiscard]] int activeGroupIndex() const { return m_activeGroup; }

    void setActiveGroup(int index);

    /// Adds a group beside the active one and makes it active. Its tab is
    /// whatever the active group was showing, which is what "split" means to a
    /// user: the same file, side by side.
    ///
    /// Returns the new group, or nullptr if the maximum is reached.
    EditorGroup* split();

    /// Removes a group, moving focus to a neighbour. The last group is never
    /// removed — an editor with no panes has nowhere to open a file.
    bool closeGroup(int index);

    /// Convenience for the common case: open into whichever group is active.
    editor::TextDocument* openInActiveGroup(const QString& path, const QString& contents);

    /// The active group's active document, or nullptr.
    [[nodiscard]] editor::TextDocument* activeDocument() const;

    /// True if any group holds unsaved work.
    [[nodiscard]] bool hasUnsavedChanges() const;

    /// Closes every tab in every group and collapses back to one group. Called
    /// when the project closes.
    void reset();

    /// More than two panes makes each too narrow to read code in at any usual
    /// window size, so the split command stops there rather than letting the
    /// user create something unusable.
    static constexpr int kMaxGroups = 2;

signals:
    void groupsChanged();
    void activeGroupChanged();

    /// The active document changed, for any reason: a tab switch, a group
    /// switch, a file opening. The editor view rebinds on this one signal
    /// rather than tracking the several ways it can happen.
    void activeDocumentChanged();

private:
    /// Wires a group's signals so the layout can republish them.
    void observe(EditorGroup* group);

    std::vector<std::unique_ptr<EditorGroup>> m_groups;
    int m_activeGroup = 0;
};

} // namespace keys::workspace
