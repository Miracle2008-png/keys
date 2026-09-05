#pragma once

#include "ui/EditorSettings.h"
#include "ui/EditorViewModel.h"
#include "ui/TabBarModel.h"
#include "workspace/EditorLayout.h"

#include <QObject>

#include <array>
#include <memory>

namespace keys::ui {

/// Keeps one view model and one tab model per editor pane, bound to the layout.
///
/// QML needs a stable object per pane: a context property cannot appear and
/// disappear as panes are created, or bindings would break rather than
/// re-evaluate. So the models for the maximum number of panes are created once
/// and rebound as the layout changes, and a pane with no group simply shows
/// nothing.
///
/// This exists so main() stays a wiring function rather than growing the logic
/// for tracking which document belongs to which pane.
class EditorBindings : public QObject {
    Q_OBJECT

public:
    EditorBindings(workspace::EditorLayout& layout, EditorSettings& settings,
                   QObject* parent = nullptr);

    [[nodiscard]] EditorViewModel* editorFor(int group) const;
    [[nodiscard]] TabBarModel* tabsFor(int group) const;

signals:
    /// A tab in `group` asked to be closed. The workspace performs it, because
    /// it owns the file watches the close has to release.
    void closeTabRequested(int group, int index);

private:
    /// Rebinds every model to whatever the layout now holds. Cheap, and called
    /// on any structural change so no caller has to work out which pane moved.
    void rebind();

    workspace::EditorLayout& m_layout;
    EditorSettings& m_settings;

    std::array<std::unique_ptr<EditorViewModel>,
               workspace::EditorLayout::kMaxGroups> m_editors;
    std::array<std::unique_ptr<TabBarModel>,
               workspace::EditorLayout::kMaxGroups> m_tabs;
};

} // namespace keys::ui
