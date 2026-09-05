#pragma once

#include "core/Result.h"
#include "debugger/DapTypes.h"
#include "langsvc/JsonRpc.h"

#include <QHash>
#include <QObject>

#include <functional>
#include <memory>

class QProcess;

namespace keys::debugger {

/// One debug session, spoken to over DAP.
///
/// **The adapter drives, not Keys.** DAP is request/response plus events, and
/// the events are what matter: the adapter says when the debuggee stopped, on
/// which thread and why. Keys never guesses at state — every transition here
/// comes from an event, which is what stops the UI showing a stack that is no
/// longer current.
///
/// **Nothing is offered that the adapter did not accept.** Capabilities come
/// back from initialize, and a breakpoint is only shown as set once the adapter
/// says it verified it. An adapter can move a breakpoint to the next executable
/// line, and showing it where the user clicked would be a lie.
///
/// **Framing is LSP's.** DAP uses the same Content-Length header, so the codec
/// is shared rather than reimplemented; only the message body differs.
class DebugSession : public QObject {
    Q_OBJECT

public:
    explicit DebugSession(QObject* parent = nullptr);
    ~DebugSession() override;

    /// Launches the adapter and starts a session. Fails when the adapter binary
    /// is missing, which is the common case since adapters are separate
    /// installs.
    core::Status start(const DebugConfig& config, const QString& projectRoot);

    /// Ends the session, politely if the adapter is still responding.
    void stop();

    [[nodiscard]] DebugState state() const { return m_state; }
    [[nodiscard]] bool isActive() const { return m_state != DebugState::Inactive; }
    [[nodiscard]] bool isStopped() const { return m_state == DebugState::Stopped; }

    [[nodiscard]] const StopInfo& stopInfo() const { return m_stopInfo; }
    [[nodiscard]] const std::vector<StackFrame>& stack() const { return m_stack; }
    [[nodiscard]] const std::vector<Variable>& variables() const { return m_variables; }

    // ---- Execution ---------------------------------------------------------
    //
    // Each is refused unless the state allows it, so a control cannot send a
    // request the adapter would reject.

    void resume();
    void pause();
    void stepOver();
    void stepInto();
    void stepOut();

    /// Selects a frame, which changes which variables are shown. Frames are how
    /// a debugger is actually read: the interesting locals are usually two or
    /// three frames up from where it stopped.
    void selectFrame(int frameId);
    [[nodiscard]] int selectedFrameId() const { return m_selectedFrameId; }

    // ---- Breakpoints -------------------------------------------------------

    /// Replaces every breakpoint in a file. DAP has no "add one" request: a
    /// source's breakpoints are set as a set, which is why the caller keeps the
    /// list and sends the whole thing.
    void setBreakpoints(const QString& sourcePath, const std::vector<int>& lines);

signals:
    void stateChanged();

    /// The debuggee stopped. The stack and variables are fetched automatically
    /// and reported through stackChanged.
    void stopped(const StopInfo& info);

    void stackChanged();
    void variablesChanged();

    /// The adapter reported where a breakpoint actually landed.
    void breakpointsVerified(const QString& sourcePath,
                             const std::vector<Breakpoint>& breakpoints);

    /// Output from the debuggee or the adapter, for the console.
    void outputReceived(const QString& text, const QString& category);

    void sessionEnded();
    void failed(const QString& message);

private:
    void send(const QJsonObject& message);
    void sendRequest(const QString& command, const QJsonObject& arguments,
                     std::function<void(const QJsonObject&)> handler = {});

    void onReadyRead();
    void handleMessage(const langsvc::RpcMessage& message);
    void handleEvent(const QString& event, const QJsonObject& body);
    void handleResponse(const QJsonObject& object);

    void setState(DebugState state);

    /// Asks for the stack, then the variables of its top frame. Chained rather
    /// than parallel because the variables request needs a frame id the stack
    /// response carries.
    void fetchStack(int threadId);
    void fetchVariables(int frameId);

    std::unique_ptr<QProcess> m_process;
    langsvc::JsonRpcCodec m_codec;

    DebugConfig m_config;
    DebugState m_state = DebugState::Inactive;
    StopInfo m_stopInfo;

    std::vector<StackFrame> m_stack;
    std::vector<Variable> m_variables;
    int m_selectedFrameId = 0;

    /// The thread that is stopped. DAP is multi-threaded; Keys follows the one
    /// the adapter reported, which is what a user means by "where am I".
    int m_stoppedThreadId = 0;

    int m_nextSeq = 1;
    QHash<int, std::function<void(const QJsonObject&)>> m_pending;

    /// Whether the adapter said it can do these. A control for something the
    /// adapter cannot do is hidden rather than shown failing.
    bool m_supportsStepBack = false;
    bool m_supportsTerminate = false;

    static constexpr int kShutdownGraceMs = 2000;
};

} // namespace keys::debugger
