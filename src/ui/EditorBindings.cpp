#include "ui/EditorBindings.h"

namespace keys::ui {

EditorBindings::EditorBindings(workspace::EditorLayout& layout, QObject* parent)
    : QObject(parent), m_layout(layout)
{
    for (int i = 0; i < workspace::EditorLayout::kMaxGroups; ++i) {
        m_editors.at(static_cast<size_t>(i)) = std::make_unique<EditorViewModel>(this);
        m_tabs.at(static_cast<size_t>(i)) = std::make_unique<TabBarModel>(this);

        // A tab's close button reaches the workspace through here, because
        // closing has to release the file's watch and the tab model does not
        // own that.
        connect(m_tabs.at(static_cast<size_t>(i)).get(),
                &TabBarModel::closeRequested, this,
                [this, i](int index) { emit closeTabRequested(i, index); });
    }

    // Any structural change rebinds everything. Working out which pane moved
    // would be more code and more ways to be wrong, for no measurable gain at
    // two panes.
    connect(&m_layout, &workspace::EditorLayout::groupsChanged,
            this, &EditorBindings::rebind);
    connect(&m_layout, &workspace::EditorLayout::activeGroupChanged,
            this, &EditorBindings::rebind);
    connect(&m_layout, &workspace::EditorLayout::activeDocumentChanged,
            this, &EditorBindings::rebind);

    rebind();
}

void EditorBindings::rebind()
{
    for (int i = 0; i < workspace::EditorLayout::kMaxGroups; ++i) {
        workspace::EditorGroup* group = m_layout.groupAt(i);

        // A pane with no group binds to nothing rather than to a stale group,
        // so a closed split cannot keep showing the document it held.
        m_tabs.at(static_cast<size_t>(i))->setGroup(group);
        m_editors.at(static_cast<size_t>(i))
            ->setDocument(group ? group->activeDocument() : nullptr);
    }
}

EditorViewModel* EditorBindings::editorFor(int group) const
{
    if (group < 0 || group >= workspace::EditorLayout::kMaxGroups) {
        return nullptr;
    }
    return m_editors.at(static_cast<size_t>(group)).get();
}

TabBarModel* EditorBindings::tabsFor(int group) const
{
    if (group < 0 || group >= workspace::EditorLayout::kMaxGroups) {
        return nullptr;
    }
    return m_tabs.at(static_cast<size_t>(group)).get();
}

} // namespace keys::ui
