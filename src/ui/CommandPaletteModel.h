#pragma once

#include "core/CommandRegistry.h"
#include "search/FileIndex.h"

#include <QAbstractListModel>
#include <QQmlEngine>

#include <vector>

namespace keys::ui {

/// The command palette's results.
///
/// **One palette, several modes.** The design shows commands and files in one
/// list, and that is what users expect: a single key opens one box that finds
/// whatever you name. Prefixes switch the mode — `>` for commands only, no
/// prefix for files — which is the convention every editor has settled on
/// because it lets the common case (find a file) need no prefix at all.
///
/// **Grouped, not interleaved.** Commands and files score on different scales
/// and interleaving them would put a mediocre command above an exact file match
/// at random. Each group is ranked internally and the groups are shown in a
/// fixed order, so the list is stable and the user learns where to look.
class CommandPaletteModel : public QAbstractListModel {
    Q_OBJECT

    // Registered as a QML singleton rather than a context property. The QML
    // module is compiled ahead of time, and the AOT compiler cannot see context
    // properties - so any binding it compiles eagerly resolves to undefined,
    // silently, with only a runtime warning. A declared singleton is visible to
    // the compiler and to QML tooling.
    QML_NAMED_ELEMENT(Palette)
    QML_SINGLETON

    // A dedicated getter rather than rowCount: moc cannot use a function with a
    // defaulted parameter as a property getter, and the property silently
    // resolves to undefined in QML rather than failing to compile.
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex
                   NOTIFY selectedIndexChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString placeholder READ placeholder NOTIFY queryChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        SubtitleRole,     ///< a command's category, or a file's directory
        GroupRole,        ///< the heading this row falls under
        IsGroupStartRole, ///< true for the first row of a group, so QML can draw its heading
        EnabledRole,
        MatchPositionsRole, ///< indices in the title the query matched, for highlighting
    };

    CommandPaletteModel(core::CommandRegistry& commands,
                        search::FileIndex& index,
                        QObject* parent = nullptr);

    /// Publishes the application's instance to QML; see Theme for the pattern.
    /// The model takes its dependencies by reference, so the engine cannot
    /// construct one itself.
    static void setInstance(CommandPaletteModel* instance);
    static CommandPaletteModel* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    /// Returned by value, matching the property's declared type. A getter
    /// returning a reference where the property says QString is a mismatch moc
    /// does not diagnose - the property just reads as undefined.
    [[nodiscard]] QString query() const { return m_query; }

    [[nodiscard]] int count() const { return static_cast<int>(m_entries.size()); }
    void setQuery(const QString& query);

    [[nodiscard]] int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);

    /// Text for the input's placeholder, which changes with the mode so the
    /// user can see what the prefix did.
    [[nodiscard]] QString placeholder() const;

    /// Moves the selection, wrapping at both ends. Wrapping matters because the
    /// list is short and reaching the bottom to find the top is a wasted press.
    Q_INVOKABLE void selectNext();
    Q_INVOKABLE void selectPrevious();

    /// Runs whatever is selected. Returns false if there is nothing to run.
    Q_INVOKABLE bool acceptSelected();

    /// Clears the query and rebuilds, for the palette opening.
    Q_INVOKABLE void reset();

signals:
    void countChanged();
    void selectedIndexChanged();
    void queryChanged();

    /// A file was chosen. The palette does not open files itself; the workspace
    /// does, so this reports the intent.
    void fileChosen(const QString& relativePath);

    /// Something was chosen and the palette should close.
    void accepted();

private:
    /// What a row represents.
    enum class Kind { Command, File };

    struct Entry {
        Kind kind = Kind::Command;
        QString title;
        QString subtitle;
        QString identifier;   ///< command id, or a file's relative path
        QString group;
        bool enabled = true;
        int score = 0;
        std::vector<int> matchPositions;
    };

    /// Rebuilds the result list for the current query.
    void rebuild();

    /// Collects and ranks matching commands.
    void appendCommands(const QString& query, std::vector<Entry>& into) const;

    /// Collects and ranks matching files.
    void appendFiles(const QString& query, std::vector<Entry>& into) const;

    core::CommandRegistry& m_commands;
    search::FileIndex& m_index;

    std::vector<Entry> m_entries;
    QString m_query;
    int m_selectedIndex = 0;

    /// The palette shows the best few, not everything. A list longer than the
    /// box can hold is scrolled past rather than read, and ranking beyond the
    /// first page is wasted work on every keystroke.
    static constexpr int kMaxCommands = 50;
    static constexpr int kMaxFiles = 100;
};

} // namespace keys::ui
