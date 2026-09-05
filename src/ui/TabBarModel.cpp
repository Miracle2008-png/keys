#include "ui/TabBarModel.h"

#include "ui/OklchColor.h"

#include <QFileInfo>
#include <QHash>

namespace keys::ui {

QColor colorForFileName(const QString& fileName)
{
    // The design maps a few common types to its syntax colours, so a file's dot
    // matches the colour its code is highlighted in. Anything unrecognised gets
    // the tertiary text colour rather than an invented hue - a palette that
    // grows a colour per extension stops meaning anything.
    static const QHash<QString, QColor> byExtension = {
        // Functions blue: the languages whose files are mostly components.
        {QStringLiteral("tsx"), oklch(0.72, 0.1, 252)},
        {QStringLiteral("jsx"), oklch(0.72, 0.1, 252)},

        // Types teal: the typed-source languages.
        {QStringLiteral("ts"), oklch(0.75, 0.08, 190)},
        {QStringLiteral("h"), oklch(0.75, 0.08, 190)},
        {QStringLiteral("hpp"), oklch(0.75, 0.08, 190)},

        // Strings green: implementation files.
        {QStringLiteral("cpp"), oklch(0.72, 0.1, 150)},
        {QStringLiteral("c"), oklch(0.72, 0.1, 150)},
        {QStringLiteral("js"), oklch(0.72, 0.1, 150)},
        {QStringLiteral("py"), oklch(0.72, 0.1, 150)},

        // Yellow: data and configuration.
        {QStringLiteral("json"), oklch(0.78, 0.12, 95)},
        {QStringLiteral("yaml"), oklch(0.78, 0.12, 95)},
        {QStringLiteral("yml"), oklch(0.78, 0.12, 95)},
        {QStringLiteral("toml"), oklch(0.78, 0.12, 95)},
        {QStringLiteral("sql"), oklch(0.78, 0.12, 95)},

        // Keyword purple: markup and QML.
        {QStringLiteral("qml"), oklch(0.68, 0.12, 300)},
        {QStringLiteral("html"), oklch(0.68, 0.12, 300)},
        {QStringLiteral("css"), oklch(0.68, 0.12, 300)},
    };

    const QString extension = QFileInfo(fileName).suffix().toLower();
    return byExtension.value(extension, oklch(0.45, 0.01, 255));
}

TabBarModel::TabBarModel(QObject* parent) : QAbstractListModel(parent) {}

void TabBarModel::setGroup(workspace::EditorGroup* group)
{
    if (m_group == group) {
        return;
    }

    beginResetModel();

    if (m_group) {
        m_group->disconnect(this);
    }

    m_group = group;

    if (m_group) {
        // A tab opening, closing or moving changes the whole list; resetting is
        // correct and cheap at the handful of tabs a bar can hold.
        connect(m_group, &workspace::EditorGroup::tabsChanged, this, [this] {
            beginResetModel();
            endResetModel();
            emit countChanged();
            emit activeIndexChanged();
        });

        connect(m_group, &workspace::EditorGroup::activeChanged,
                this, &TabBarModel::activeIndexChanged);

        // A dirty-state change repaints one tab rather than the bar.
        connect(m_group, &workspace::EditorGroup::tabModifiedChanged, this,
                [this](int row) {
                    if (row >= 0 && row < rowCount()) {
                        const QModelIndex changed = index(row, 0);
                        emit dataChanged(changed, changed, {ModifiedRole});
                    }
                });
    }

    endResetModel();
    emit countChanged();
    emit activeIndexChanged();
}

int TabBarModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_group) {
        return 0;
    }
    return m_group->tabCount();
}

QVariant TabBarModel::data(const QModelIndex& index, int role) const
{
    if (!m_group || !index.isValid()) {
        return {};
    }

    const editor::TextDocument* document = m_group->documentAt(index.row());
    if (!document) {
        return {};
    }

    const QString fileName = QFileInfo(document->path()).fileName();

    switch (role) {
    case NameRole:
        // An unsaved document has no file name; call it what it is rather than
        // showing an empty tab.
        return fileName.isEmpty() ? tr("Untitled") : fileName;
    case PathRole:
        return document->path();
    case ModifiedRole:
        return document->isModified();
    case AccentRole:
        return colorForFileName(fileName);
    default:
        return {};
    }
}

QHash<int, QByteArray> TabBarModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {ModifiedRole, "modified"},
        {AccentRole, "accent"},
    };
}

int TabBarModel::activeIndex() const
{
    return m_group ? m_group->activeIndex() : -1;
}

void TabBarModel::activate(int index)
{
    if (m_group) {
        m_group->setActiveIndex(index);
    }
}

void TabBarModel::close(int index)
{
    // The workspace owns file watches and other per-file state, so closing is
    // requested rather than performed here.
    emit closeRequested(index);
}

void TabBarModel::move(int from, int to)
{
    if (m_group) {
        m_group->moveTab(from, to);
    }
}

} // namespace keys::ui
