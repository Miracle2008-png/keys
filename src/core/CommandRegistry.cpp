#include "core/CommandRegistry.h"

#include "core/Log.h"

#include <algorithm>

namespace keys::core {

CommandRegistry::CommandRegistry(QObject* parent) : QObject(parent) {}

Status CommandRegistry::registerCommand(Command command)
{
    if (command.id.isEmpty()) {
        return Err(ErrorCode::InvalidArgument, QStringLiteral("Command id must not be empty"));
    }
    if (!command.handler) {
        return Err(ErrorCode::InvalidArgument,
                   QStringLiteral("Command has no handler"),
                   command.id);
    }
    if (m_commands.contains(command.id)) {
        // Two modules claiming one id is a wiring bug. Surfacing it at startup is
        // far cheaper than debugging whichever registration happened to win.
        return Err(ErrorCode::AlreadyExists,
                   QStringLiteral("A command with this id is already registered"),
                   command.id);
    }

    const QString id = command.id;
    m_commands.insert(id, std::move(command));
    m_order.append(id);

    emit commandRegistered(id);
    return Ok();
}

Status CommandRegistry::unregisterCommand(const QString& id)
{
    if (m_commands.remove(id) == 0) {
        return Err(ErrorCode::NotFound, QStringLiteral("No such command"), id);
    }
    m_order.removeOne(id);

    emit commandUnregistered(id);
    return Ok();
}

bool CommandRegistry::contains(const QString& id) const
{
    return m_commands.contains(id);
}

const Command* CommandRegistry::find(const QString& id) const
{
    const auto it = m_commands.constFind(id);
    return it == m_commands.cend() ? nullptr : &it.value();
}

QList<Command> CommandRegistry::all() const
{
    QList<Command> result;
    result.reserve(m_order.size());
    for (const QString& id : m_order) {
        const auto it = m_commands.constFind(id);
        if (it != m_commands.cend()) {
            result.append(it.value());
        }
    }

    // Category then title, with registration order breaking ties. A stable order
    // means palette entries do not move between openings.
    std::stable_sort(result.begin(), result.end(), [](const Command& a, const Command& b) {
        if (a.category != b.category) {
            return a.category < b.category;
        }
        return a.title < b.title;
    });
    return result;
}

Status CommandRegistry::invoke(const QString& id)
{
    const Command* command = find(id);
    if (!command) {
        return Err(ErrorCode::NotFound, QStringLiteral("No such command"), id);
    }
    if (!command->enabled()) {
        return Err(ErrorCode::NotSupported,
                   QStringLiteral("Command is not available right now"),
                   id);
    }

    qCDebug(lcCore) << "invoking command" << id;

    // Copy the handler before calling: a command that unregisters itself (or its
    // module) would otherwise destroy the object we are executing from.
    const auto handler = command->handler;
    handler();
    return Ok();
}

} // namespace keys::core
