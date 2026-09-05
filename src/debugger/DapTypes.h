#pragma once

#include <QJsonObject>
#include <QString>

#include <vector>

namespace keys::debugger {

/// What state the debuggee is in.
///
/// Every control in the debug UI is enabled from this, so a button never offers
/// something the adapter would reject: Continue means nothing while running, and
/// Step means nothing while there is no stopped thread.
enum class DebugState {
    Inactive,     ///< no session
    Starting,     ///< adapter launched, initialize sent
    Running,      ///< the debuggee is executing
    Stopped,      ///< paused at a breakpoint, step or exception
    Terminating,
};

/// Why the debuggee stopped. DAP's own reason strings, kept as text because
/// adapters invent their own beyond the documented set.
struct StopInfo {
    QString reason;        ///< "breakpoint", "step", "exception", "pause"
    QString description;   ///< human text, when the adapter provides it
    int threadId = 0;
};

/// One frame in the call stack.
struct StackFrame {
    int id = 0;            ///< the adapter's handle, used to ask for its scopes
    QString name;
    QString sourcePath;    ///< empty for a frame with no source (a system library)
    int line = 0;          ///< one-based, as DAP uses
    int column = 0;

    [[nodiscard]] bool hasSource() const { return !sourcePath.isEmpty(); }
    [[nodiscard]] static StackFrame fromJson(const QJsonObject& object);
};

/// A variable, or a scope containing them.
struct Variable {
    QString name;
    QString value;
    QString type;

    /// Non-zero when this can be expanded. DAP calls it a variablesReference:
    /// asking for that reference returns the children.
    int variablesReference = 0;

    [[nodiscard]] bool isExpandable() const { return variablesReference != 0; }
    [[nodiscard]] static Variable fromJson(const QJsonObject& object);
};

/// A breakpoint the user set.
struct Breakpoint {
    QString sourcePath;
    int line = 0;          ///< one-based

    /// Whether the adapter accepted it. An adapter can refuse a breakpoint or
    /// move it to the next executable line, and the UI must show where it
    /// actually landed rather than where it was clicked.
    bool verified = false;

    /// Where the adapter put it, when that differs from `line`.
    int actualLine = 0;

    [[nodiscard]] int effectiveLine() const { return actualLine > 0 ? actualLine : line; }
};

/// How a debug session is launched.
struct DebugConfig {
    QString name;
    QString adapterProgram;      ///< the DAP adapter binary
    QStringList adapterArguments;

    /// The launch request's arguments, adapter-specific. Passed through rather
    /// than modelled: every adapter defines its own, and inventing a common
    /// shape would fit none of them.
    QJsonObject launchArguments;

    [[nodiscard]] bool isValid() const
    {
        return !name.isEmpty() && !adapterProgram.isEmpty();
    }
};

[[nodiscard]] QString debugStateLabel(DebugState state);

} // namespace keys::debugger
