#pragma once

#include "core/Result.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace keys::core {

/// A user-invokable action.
///
/// One registry owns every command in the application. The palette, the keybinding
/// table, menus and (later) extension contributions are all views over this single
/// registry — so a command is defined once and reachable every way, rather than
/// wired separately per surface.
struct Command {
    /// Stable, namespaced identifier: "workspace.openFolder", "editor.format".
    /// Keybindings and extensions reference commands by id, so ids are API: they
    /// outlive titles and must not change casually.
    QString id;

    /// Shown in the palette. Sentence case, no trailing punctuation.
    QString title;

    /// Groups related commands in the palette ("File", "Editor", "Git").
    QString category;

    /// Extra words to match against in the palette that do not belong in the
    /// title — e.g. "folder" for "Open Project".
    QStringList keywords;

    /// What running the command does.
    std::function<void()> handler;

    /// Whether the command can run right now. A command with no predicate is
    /// always enabled. Disabled commands stay visible but unselectable rather
    /// than vanishing, so the palette does not shift under the user.
    std::function<bool()> isEnabled;

    [[nodiscard]] bool enabled() const { return !isEnabled || isEnabled(); }
};

/// The application's command registry.
///
/// Modules register their own commands at startup. The registry itself knows
/// nothing about what any command does, which is what lets modules contribute
/// without the registry growing a dependency on them.
class CommandRegistry : public QObject {
    Q_OBJECT

public:
    explicit CommandRegistry(QObject* parent = nullptr);

    /// Registers a command. Fails on a duplicate id or an empty id/handler rather
    /// than silently overwriting — a duplicate id means two modules disagree about
    /// ownership, which should surface at startup, not as a mystery later.
    Status registerCommand(Command command);

    /// Removes a command. Used when an extension deactivates.
    Status unregisterCommand(const QString& id);

    [[nodiscard]] bool contains(const QString& id) const;
    [[nodiscard]] const Command* find(const QString& id) const;

    /// Every registered command, ordered by category then title so the palette has
    /// a stable presentation without sorting at every keystroke.
    [[nodiscard]] QList<Command> all() const;

    [[nodiscard]] int count() const { return static_cast<int>(m_commands.size()); }

    /// Invokes a command by id. Fails if unknown or currently disabled; the caller
    /// gets a reason rather than a silent no-op.
    Status invoke(const QString& id);

signals:
    void commandRegistered(const QString& id);
    void commandUnregistered(const QString& id);

private:
    QHash<QString, Command> m_commands;
    QStringList m_order;  ///< registration order, for stable sorting of equal keys
};

} // namespace keys::core
