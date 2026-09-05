#pragma once

#include "editor/TextDocument.h"

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

namespace keys::workspace {

/// One editor pane and the tabs open in it.
///
/// A group owns its documents. Splitting the editor creates a second group, and
/// the same file opened in both groups gets one document per group rather than a
/// shared one — which is the simpler and more predictable model: each pane has
/// its own caret, scroll position and undo history, and closing a tab in one
/// pane cannot disturb the other.
///
/// The alternative — one document shared by every view of a file — avoids
/// divergent copies but requires reconciling two carets and two undo stacks over
/// one buffer. That is the harder half of a collaborative editor, and it is not
/// what a split view is for. Milestone 12 revisits this if language features
/// make per-group documents costly.
class EditorGroup : public QObject {
    Q_OBJECT

public:
    explicit EditorGroup(QObject* parent = nullptr);
    ~EditorGroup() override;

    /// Opens `path`, or activates the tab if it is already open. Returns the
    /// document, or nullptr if the file could not be read.
    editor::TextDocument* openFile(const QString& path, const QString& contents);

    /// Closes the tab at `index`. Returns false if the index is out of range.
    /// The caller is responsible for asking about unsaved changes first; this
    /// discards them.
    bool closeTab(int index);

    /// Closes every tab.
    void closeAll();

    /// Moves a tab, for drag reordering.
    bool moveTab(int from, int to);

    [[nodiscard]] int tabCount() const { return static_cast<int>(m_documents.size()); }
    [[nodiscard]] bool isEmpty() const { return m_documents.empty(); }

    [[nodiscard]] int activeIndex() const { return m_activeIndex; }
    void setActiveIndex(int index);

    /// The document at `index`, or nullptr if out of range.
    [[nodiscard]] editor::TextDocument* documentAt(int index) const;

    /// The active document, or nullptr when the group is empty.
    [[nodiscard]] editor::TextDocument* activeDocument() const;

    /// Index of the tab showing `path`, or -1.
    [[nodiscard]] int indexOfPath(const QString& path) const;

    /// True if any tab has unsaved changes — asked before closing the group.
    [[nodiscard]] bool hasUnsavedChanges() const;

signals:
    /// The set of tabs changed: one opened, closed or moved.
    void tabsChanged();

    /// A different tab became active.
    void activeChanged();

    /// A tab's dirty state changed, so its indicator needs redrawing.
    void tabModifiedChanged(int index);

private:
    /// Connects a document's signals to this group's, so the tab bar learns
    /// about dirty state without every caller wiring it up.
    void observe(editor::TextDocument* document);

    std::vector<std::unique_ptr<editor::TextDocument>> m_documents;

    /// -1 when the group is empty.
    int m_activeIndex = -1;
};

} // namespace keys::workspace
