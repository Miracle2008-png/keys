#include "debugger/DapTypes.h"

namespace keys::debugger {

StackFrame StackFrame::fromJson(const QJsonObject& object)
{
    StackFrame frame;
    frame.id = object.value(QStringLiteral("id")).toInt();
    frame.name = object.value(QStringLiteral("name")).toString();
    frame.line = object.value(QStringLiteral("line")).toInt();
    frame.column = object.value(QStringLiteral("column")).toInt();

    // A frame need not have source: a stack that walks into a system library
    // has frames the user cannot open, and they still belong in the list so the
    // call chain reads correctly.
    frame.sourcePath =
        object.value(QStringLiteral("source")).toObject()
            .value(QStringLiteral("path")).toString();

    return frame;
}

Variable Variable::fromJson(const QJsonObject& object)
{
    Variable variable;
    variable.name = object.value(QStringLiteral("name")).toString();
    variable.value = object.value(QStringLiteral("value")).toString();
    variable.type = object.value(QStringLiteral("type")).toString();
    variable.variablesReference =
        object.value(QStringLiteral("variablesReference")).toInt();
    return variable;
}

QString debugStateLabel(DebugState state)
{
    switch (state) {
    case DebugState::Inactive:
        return QStringLiteral("Not running");
    case DebugState::Starting:
        return QStringLiteral("Starting");
    case DebugState::Running:
        return QStringLiteral("Running");
    case DebugState::Stopped:
        return QStringLiteral("Paused");
    case DebugState::Terminating:
        return QStringLiteral("Stopping");
    }
    return QString();
}

} // namespace keys::debugger
