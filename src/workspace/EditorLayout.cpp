#include "workspace/EditorLayout.h"

#include <algorithm>

namespace keys::workspace {

EditorLayout::EditorLayout(QObject* parent) : QObject(parent)
{
    // There is always at least one group: an editor with no panes has nowhere
    // to open a file, and every caller would need a null check.
    auto group = std::make_unique<EditorGroup>(this);
    observe(group.get());
    m_groups.push_back(std::move(group));
}

EditorLayout::~EditorLayout() = default;

void EditorLayout::observe(EditorGroup* group)
{
    // Any change to which document is active — a tab switch inside a group, a
    // tab opening or closing — republishes as one signal, so the editor view
    // rebinds without tracking each cause separately.
    connect(group, &EditorGroup::activeChanged, this, [this, group] {
        if (group == activeGroup()) {
            emit activeDocumentChanged();
        }
    });

    connect(group, &EditorGroup::tabsChanged, this, &EditorLayout::groupsChanged);
}

EditorGroup* EditorLayout::groupAt(int index) const
{
    if (index < 0 || index >= groupCount()) {
        return nullptr;
    }
    return m_groups.at(static_cast<size_t>(index)).get();
}

EditorGroup* EditorLayout::activeGroup() const
{
    return groupAt(m_activeGroup);
}

void EditorLayout::setActiveGroup(int index)
{
    if (index == m_activeGroup || index < 0 || index >= groupCount()) {
        return;
    }

    m_activeGroup = index;

    emit activeGroupChanged();
    emit activeDocumentChanged();
}

EditorGroup* EditorLayout::split()
{
    if (groupCount() >= kMaxGroups) {
        return nullptr;
    }

    auto group = std::make_unique<EditorGroup>(this);
    observe(group.get());

    EditorGroup* raw = group.get();
    m_groups.push_back(std::move(group));

    // The new pane shows what the old one was showing. Splitting to an empty
    // pane would make the user re-open the file they were already looking at,
    // which is not what the gesture means.
    if (const EditorGroup* source = activeGroup()) {
        if (const editor::TextDocument* document = source->activeDocument()) {
            raw->openFile(document->path(), document->text());
        }
    }

    m_activeGroup = groupCount() - 1;

    emit groupsChanged();
    emit activeGroupChanged();
    emit activeDocumentChanged();
    return raw;
}

bool EditorLayout::closeGroup(int index)
{
    // Never remove the last group.
    if (groupCount() <= 1 || index < 0 || index >= groupCount()) {
        return false;
    }

    m_groups.erase(m_groups.begin() + index);

    // Move focus to a neighbour rather than resetting to the first pane.
    if (m_activeGroup >= groupCount()) {
        m_activeGroup = groupCount() - 1;
    }

    emit groupsChanged();
    emit activeGroupChanged();
    emit activeDocumentChanged();
    return true;
}

editor::TextDocument* EditorLayout::openInActiveGroup(const QString& path,
                                                      const QString& contents)
{
    EditorGroup* group = activeGroup();
    if (!group) {
        return nullptr;
    }

    editor::TextDocument* document = group->openFile(path, contents);
    emit activeDocumentChanged();
    return document;
}

editor::TextDocument* EditorLayout::activeDocument() const
{
    const EditorGroup* group = activeGroup();
    return group ? group->activeDocument() : nullptr;
}

bool EditorLayout::hasUnsavedChanges() const
{
    return std::any_of(m_groups.begin(), m_groups.end(),
                       [](const std::unique_ptr<EditorGroup>& group) {
                           return group->hasUnsavedChanges();
                       });
}

void EditorLayout::reset()
{
    // Collapse back to a single empty group. Erasing from the end avoids
    // shifting the elements that are about to be erased anyway.
    while (groupCount() > 1) {
        m_groups.pop_back();
    }

    m_groups.front()->closeAll();
    m_activeGroup = 0;

    emit groupsChanged();
    emit activeGroupChanged();
    emit activeDocumentChanged();
}

} // namespace keys::workspace
