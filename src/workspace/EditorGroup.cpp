#include "workspace/EditorGroup.h"

#include "filesystem/FileSystem.h"

#include <algorithm>

namespace keys::workspace {

EditorGroup::EditorGroup(QObject* parent) : QObject(parent) {}

EditorGroup::~EditorGroup() = default;

void EditorGroup::observe(editor::TextDocument* document)
{
    // The tab bar needs to redraw a tab's dirty dot the moment it changes, but
    // it addresses tabs by index - which shifts as tabs open and close. Looking
    // the index up at emit time rather than capturing it keeps the signal
    // correct after a reorder.
    connect(document, &editor::TextDocument::modifiedChanged, this,
            [this, document] {
                const int index = indexOfPath(document->path());
                if (index >= 0) {
                    emit tabModifiedChanged(index);
                }
            });
}

editor::TextDocument* EditorGroup::openFile(const QString& path, const QString& contents)
{
    const QString normalized = fs::FileSystem::normalize(path);

    // Already open: activate rather than opening a second copy. Two tabs for one
    // file in the same pane would give the user two carets and two undo stacks
    // over the same bytes, and no way to tell which one saves.
    if (const int existing = indexOfPath(normalized); existing >= 0) {
        setActiveIndex(existing);
        return m_documents.at(static_cast<size_t>(existing)).get();
    }

    auto document = std::make_unique<editor::TextDocument>();
    document->setText(contents);
    document->setPath(normalized);

    editor::TextDocument* raw = document.get();
    observe(raw);

    // New tabs open after the active one, not at the end. Opening a file while
    // working keeps it next to what it relates to rather than at the far right
    // of a long tab bar.
    const int insertAt = m_activeIndex >= 0 ? m_activeIndex + 1
                                            : static_cast<int>(m_documents.size());
    m_documents.insert(m_documents.begin() + insertAt, std::move(document));

    m_activeIndex = insertAt;

    emit tabsChanged();
    emit activeChanged();
    return raw;
}

bool EditorGroup::closeTab(int index)
{
    if (index < 0 || index >= tabCount()) {
        return false;
    }

    m_documents.erase(m_documents.begin() + index);

    // Activate the tab that took its place, or the one before it if the closed
    // tab was last. Falling back to index 0 would jump the user across the bar.
    if (m_documents.empty()) {
        m_activeIndex = -1;
    } else if (m_activeIndex > index) {
        --m_activeIndex;
    } else if (m_activeIndex == index) {
        m_activeIndex = std::min(index, tabCount() - 1);
    }

    emit tabsChanged();
    emit activeChanged();
    return true;
}

void EditorGroup::closeAll()
{
    if (m_documents.empty()) {
        return;
    }

    m_documents.clear();
    m_activeIndex = -1;

    emit tabsChanged();
    emit activeChanged();
}

bool EditorGroup::moveTab(int from, int to)
{
    if (from == to || from < 0 || from >= tabCount() || to < 0 || to >= tabCount()) {
        return false;
    }

    // Remember which document was active by identity, because its index is
    // about to change.
    const editor::TextDocument* active = activeDocument();

    auto document = std::move(m_documents.at(static_cast<size_t>(from)));
    m_documents.erase(m_documents.begin() + from);
    m_documents.insert(m_documents.begin() + to, std::move(document));

    if (active) {
        for (int i = 0; i < tabCount(); ++i) {
            if (m_documents.at(static_cast<size_t>(i)).get() == active) {
                m_activeIndex = i;
                break;
            }
        }
    }

    emit tabsChanged();
    return true;
}

void EditorGroup::setActiveIndex(int index)
{
    if (index == m_activeIndex || index < 0 || index >= tabCount()) {
        return;
    }
    m_activeIndex = index;
    emit activeChanged();
}

editor::TextDocument* EditorGroup::documentAt(int index) const
{
    if (index < 0 || index >= tabCount()) {
        return nullptr;
    }
    return m_documents.at(static_cast<size_t>(index)).get();
}

editor::TextDocument* EditorGroup::activeDocument() const
{
    return documentAt(m_activeIndex);
}

int EditorGroup::indexOfPath(const QString& path) const
{
    const QString normalized = fs::FileSystem::normalize(path);
    for (int i = 0; i < tabCount(); ++i) {
        if (m_documents.at(static_cast<size_t>(i))->path() == normalized) {
            return i;
        }
    }
    return -1;
}

bool EditorGroup::hasUnsavedChanges() const
{
    return std::any_of(m_documents.begin(), m_documents.end(),
                       [](const std::unique_ptr<editor::TextDocument>& document) {
                           return document->isModified();
                       });
}

} // namespace keys::workspace
