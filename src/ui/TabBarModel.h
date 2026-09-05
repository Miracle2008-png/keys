#pragma once

#include "workspace/EditorGroup.h"

#include <QAbstractListModel>
#include <QColor>

namespace keys::ui {

/// The tabs of one editor group, as a list model.
///
/// A model rather than a QML Repeater over a plain list, so opening, closing and
/// reordering animate as row insertions and moves instead of rebuilding the bar
/// — which is what keeps a tab from flickering when its neighbour closes.
class TabBarModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        ModifiedRole,
        AccentRole,   ///< the file-type dot's colour
    };

    explicit TabBarModel(QObject* parent = nullptr);

    /// Binds to a group. Passing nullptr empties the model, which is what
    /// happens when a split pane closes.
    void setGroup(workspace::EditorGroup* group);
    [[nodiscard]] workspace::EditorGroup* group() const { return m_group; }

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int activeIndex() const;

    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void close(int index);
    Q_INVOKABLE void move(int from, int to);

signals:
    void activeIndexChanged();
    void countChanged();

    /// A tab was closed and the caller must release its file watch and any
    /// other per-file state. The model does not own that.
    void closeRequested(int index);

private:
    workspace::EditorGroup* m_group = nullptr;
};

/// The colour of a file type's dot in the tab bar and, later, the explorer.
///
/// Free function rather than a member so the explorer can use the same mapping
/// without depending on the tab bar. The colours come from the design's syntax
/// palette, so a file's dot matches the colour its code is highlighted in.
[[nodiscard]] QColor colorForFileName(const QString& fileName);

} // namespace keys::ui
