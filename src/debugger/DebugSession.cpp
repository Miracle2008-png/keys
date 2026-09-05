#include "debugger/DebugSession.h"

#include "core/Log.h"

#include <QJsonArray>
#include <QProcess>

using keys::langsvc::JsonRpcCodec;
using keys::langsvc::RpcMessage;

namespace keys::debugger {

DebugSession::DebugSession(QObject* parent) : QObject(parent) {}

DebugSession::~DebugSession()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // A debuggee left running after Keys exits is a stopped process nobody
        // can resume, holding whatever it had open.
        m_process->kill();
        m_process->waitForFinished(kShutdownGraceMs);
    }
}

void DebugSession::setState(DebugState state)
{
    if (state == m_state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

core::Status DebugSession::start(const DebugConfig& config, const QString& projectRoot)
{
    if (isActive()) {
        return core::Err(core::ErrorCode::AlreadyExists,
                         QStringLiteral("A debug session is already running"),
                         m_config.name);
    }
    if (!config.isValid()) {
        return core::Err(core::ErrorCode::InvalidArgument,
                         QStringLiteral("The debug configuration is incomplete"),
                         config.name);
    }

    m_config = config;
    m_codec.reset();
    m_pending.clear();
    m_stack.clear();
    m_variables.clear();
    m_stopInfo = {};
    m_selectedFrameId = 0;
    m_stoppedThreadId = 0;
    m_nextSeq = 1;

    m_process = std::make_unique<QProcess>();
    m_process->setWorkingDirectory(projectRoot);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process.get(), &QProcess::readyReadStandardOutput,
            this, &DebugSession::onReadyRead);

    connect(m_process.get(), &QProcess::readyReadStandardError, this, [this] {
        const QString text = QString::fromUtf8(m_process->readAllStandardError());
        if (!text.trimmed().isEmpty()) {
            // The adapter's own log, not the debuggee's output. Shown in the
            // console because an adapter that is refusing to work usually says
            // why here.
            emit outputReceived(text, QStringLiteral("adapter"));
        }
    });

    connect(m_process.get(), &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart) {
                    return;
                }
                // Adapters are separate installs, so this is the common failure.
                const QString message =
                    tr("Could not start the debug adapter '%1'. Is it installed "
                       "and on your PATH?")
                        .arg(m_config.adapterProgram);
                setState(DebugState::Inactive);
                emit failed(message);
            });

    connect(m_process.get(), &QProcess::finished, this, [this] {
        // Every pending request is now unanswerable.
        m_pending.clear();
        m_stack.clear();
        m_variables.clear();

        setState(DebugState::Inactive);
        emit stackChanged();
        emit variablesChanged();
        emit sessionEnded();
    });

    setState(DebugState::Starting);
    m_process->start(config.adapterProgram, config.adapterArguments);

    // initialize, then launch. The order is fixed by the protocol: an adapter
    // rejects a launch before it has been initialized.
    sendRequest(QStringLiteral("initialize"),
                {{QStringLiteral("clientID"), QStringLiteral("keys")},
                 {QStringLiteral("adapterID"), config.name},
                 {QStringLiteral("linesStartAt1"), true},
                 {QStringLiteral("columnsStartAt1"), true},
                 {QStringLiteral("pathFormat"), QStringLiteral("path")},
                 {QStringLiteral("supportsVariableType"), true}},
                [this](const QJsonObject& body) {
                    m_supportsStepBack =
                        body.value(QStringLiteral("supportsStepBack")).toBool();
                    m_supportsTerminate =
                        body.value(QStringLiteral("supportsTerminateRequest")).toBool();

                    sendRequest(QStringLiteral("launch"), m_config.launchArguments);
                });

    return core::Ok();
}

void DebugSession::stop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        setState(DebugState::Inactive);
        return;
    }

    setState(DebugState::Terminating);

    // terminate asks the debuggee to end cleanly; disconnect ends the session
    // regardless. An adapter that supports neither is killed below.
    if (m_supportsTerminate) {
        sendRequest(QStringLiteral("terminate"), {});
    } else {
        sendRequest(QStringLiteral("disconnect"),
                    {{QStringLiteral("terminateDebuggee"), true}});
    }

    if (!m_process->waitForFinished(kShutdownGraceMs)) {
        qCWarning(lcCore) << m_config.adapterProgram << "ignored terminate; killing";
        m_process->kill();
        m_process->waitForFinished(kShutdownGraceMs);
    }
    setState(DebugState::Inactive);
}

void DebugSession::send(const QJsonObject& message)
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }
    m_process->write(JsonRpcCodec::encode(message));
}

void DebugSession::sendRequest(const QString& command, const QJsonObject& arguments,
                               std::function<void(const QJsonObject&)> handler)
{
    const int seq = m_nextSeq++;
    if (handler) {
        m_pending.insert(seq, std::move(handler));
    }

    // DAP's envelope, which is not JSON-RPC's: `seq` and `type` rather than
    // `id` and `jsonrpc`. Only the framing is shared.
    send({{QStringLiteral("seq"), seq},
          {QStringLiteral("type"), QStringLiteral("request")},
          {QStringLiteral("command"), command},
          {QStringLiteral("arguments"), arguments}});
}

void DebugSession::onReadyRead()
{
    m_codec.append(m_process->readAllStandardOutput());

    while (const std::optional<RpcMessage> message = m_codec.next()) {
        handleMessage(*message);
    }
}

void DebugSession::handleMessage(const RpcMessage& message)
{
    // DAP's envelope, read from the raw object: `type` says which of the three
    // message shapes this is, and the codec only decoded the framing.
    const QJsonObject& object = message.raw;
    const QString type = object.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("event")) {
        handleEvent(object.value(QStringLiteral("event")).toString(),
                    object.value(QStringLiteral("body")).toObject());
        return;
    }

    if (type == QLatin1String("response")) {
        handleResponse(object);
        return;
    }

    // An adapter can send a request of its own ("runInTerminal"). Keys does not
    // implement any, so this answers rather than leaving the adapter waiting.
    if (type == QLatin1String("request")) {
        send({{QStringLiteral("seq"), m_nextSeq++},
              {QStringLiteral("type"), QStringLiteral("response")},
              {QStringLiteral("request_seq"), object.value(QStringLiteral("seq"))},
              {QStringLiteral("command"), object.value(QStringLiteral("command"))},
              {QStringLiteral("success"), false},
              {QStringLiteral("message"), QStringLiteral("Not supported by Keys")}});
    }
}

void DebugSession::handleEvent(const QString& event, const QJsonObject& body)
{
    if (event == QLatin1String("initialized")) {
        // The adapter is ready for breakpoints. configurationDone tells it to
        // start running; without it, several adapters wait forever.
        sendRequest(QStringLiteral("configurationDone"), {});
        return;
    }

    if (event == QLatin1String("stopped")) {
        m_stopInfo.reason = body.value(QStringLiteral("reason")).toString();
        m_stopInfo.description = body.value(QStringLiteral("description")).toString();
        m_stopInfo.threadId = body.value(QStringLiteral("threadId")).toInt();
        m_stoppedThreadId = m_stopInfo.threadId;

        setState(DebugState::Stopped);
        emit stopped(m_stopInfo);

        // The stack is what the user needs immediately; the variables follow
        // from its top frame.
        fetchStack(m_stoppedThreadId);
        return;
    }

    if (event == QLatin1String("continued")) {
        m_stack.clear();
        m_variables.clear();
        setState(DebugState::Running);
        emit stackChanged();
        emit variablesChanged();
        return;
    }

    if (event == QLatin1String("output")) {
        emit outputReceived(body.value(QStringLiteral("output")).toString(),
                            body.value(QStringLiteral("category")).toString());
        return;
    }

    if (event == QLatin1String("terminated") || event == QLatin1String("exited")) {
        setState(DebugState::Inactive);
        emit sessionEnded();
        return;
    }

    if (event == QLatin1String("breakpoint")) {
        // The adapter moved or verified a breakpoint after the fact, which
        // happens once it has loaded the module the breakpoint is in.
        const QJsonObject breakpoint = body.value(QStringLiteral("breakpoint")).toObject();

        Breakpoint result;
        result.verified = breakpoint.value(QStringLiteral("verified")).toBool();
        result.actualLine = breakpoint.value(QStringLiteral("line")).toInt();
        result.sourcePath = breakpoint.value(QStringLiteral("source")).toObject()
                                .value(QStringLiteral("path")).toString();
        result.line = result.actualLine;

        if (!result.sourcePath.isEmpty()) {
            emit breakpointsVerified(result.sourcePath, {result});
        }
    }
}

void DebugSession::handleResponse(const QJsonObject& object)
{
    const int requestSeq = object.value(QStringLiteral("request_seq")).toInt();
    const auto handler = m_pending.take(requestSeq);

    if (!object.value(QStringLiteral("success")).toBool()) {
        // A failed request is reported rather than silently dropped: a refused
        // launch is the difference between "not started" and "started and
        // immediately stopped", and the user needs to know which.
        const QString message = object.value(QStringLiteral("message")).toString();
        qCWarning(lcCore) << "debug adapter refused"
                          << object.value(QStringLiteral("command")).toString()
                          << ":" << message;

        if (object.value(QStringLiteral("command")).toString()
            == QLatin1String("launch")) {
            emit failed(tr("The debugger could not start: %1").arg(message));
            setState(DebugState::Inactive);
        }
        return;
    }

    if (handler) {
        handler(object.value(QStringLiteral("body")).toObject());
    }
}

void DebugSession::fetchStack(int threadId)
{
    sendRequest(QStringLiteral("stackTrace"),
                {{QStringLiteral("threadId"), threadId},
                 // Bounded: a runaway recursion produces a stack thousands deep
                 // and nobody reads past the first screen.
                 {QStringLiteral("levels"), 64}},
                [this](const QJsonObject& body) {
                    m_stack.clear();

                    const QJsonArray frames =
                        body.value(QStringLiteral("stackFrames")).toArray();
                    m_stack.reserve(static_cast<size_t>(frames.size()));
                    for (const QJsonValue& frame : frames) {
                        m_stack.push_back(StackFrame::fromJson(frame.toObject()));
                    }

                    emit stackChanged();

                    // The top frame is where execution is, so its locals are
                    // what the user wants without asking.
                    if (!m_stack.empty()) {
                        m_selectedFrameId = m_stack.front().id;
                        fetchVariables(m_selectedFrameId);
                    }
                });
}

void DebugSession::fetchVariables(int frameId)
{
    // Scopes first: a frame has several (Locals, Arguments, Globals), each with
    // its own reference to expand.
    sendRequest(QStringLiteral("scopes"),
                {{QStringLiteral("frameId"), frameId}},
                [this](const QJsonObject& body) {
                    const QJsonArray scopes = body.value(QStringLiteral("scopes")).toArray();
                    if (scopes.isEmpty()) {
                        m_variables.clear();
                        emit variablesChanged();
                        return;
                    }

                    // The first scope, which every adapter puts Locals in.
                    // Showing every scope needs a tree, which is beyond what
                    // this milestone covers, and locals are what is actually
                    // read while stepping.
                    const int reference =
                        scopes.first().toObject()
                            .value(QStringLiteral("variablesReference")).toInt();

                    sendRequest(QStringLiteral("variables"),
                                {{QStringLiteral("variablesReference"), reference}},
                                [this](const QJsonObject& variablesBody) {
                                    m_variables.clear();

                                    const QJsonArray items =
                                        variablesBody.value(QStringLiteral("variables"))
                                            .toArray();
                                    m_variables.reserve(static_cast<size_t>(items.size()));
                                    for (const QJsonValue& item : items) {
                                        m_variables.push_back(
                                            Variable::fromJson(item.toObject()));
                                    }
                                    emit variablesChanged();
                                });
                });
}

void DebugSession::selectFrame(int frameId)
{
    if (frameId == m_selectedFrameId || !isStopped()) {
        return;
    }
    m_selectedFrameId = frameId;
    fetchVariables(frameId);
}

// ---- Execution -------------------------------------------------------------

void DebugSession::resume()
{
    if (!isStopped()) {
        return;   // refused rather than sent: the adapter would reject it
    }
    sendRequest(QStringLiteral("continue"),
                {{QStringLiteral("threadId"), m_stoppedThreadId}});

    // Optimistic: the adapter sends `continued` only sometimes, and a UI that
    // waited for it would sit showing a stale stack.
    m_stack.clear();
    m_variables.clear();
    setState(DebugState::Running);
    emit stackChanged();
    emit variablesChanged();
}

void DebugSession::pause()
{
    if (m_state != DebugState::Running) {
        return;
    }
    sendRequest(QStringLiteral("pause"),
                {{QStringLiteral("threadId"), m_stoppedThreadId}});
}

void DebugSession::stepOver()
{
    if (!isStopped()) {
        return;
    }
    sendRequest(QStringLiteral("next"), {{QStringLiteral("threadId"), m_stoppedThreadId}});
    setState(DebugState::Running);
}

void DebugSession::stepInto()
{
    if (!isStopped()) {
        return;
    }
    sendRequest(QStringLiteral("stepIn"), {{QStringLiteral("threadId"), m_stoppedThreadId}});
    setState(DebugState::Running);
}

void DebugSession::stepOut()
{
    if (!isStopped()) {
        return;
    }
    sendRequest(QStringLiteral("stepOut"), {{QStringLiteral("threadId"), m_stoppedThreadId}});
    setState(DebugState::Running);
}

void DebugSession::setBreakpoints(const QString& sourcePath, const std::vector<int>& lines)
{
    if (!isActive()) {
        return;
    }

    QJsonArray breakpoints;
    for (const int line : lines) {
        breakpoints.append(QJsonObject{{QStringLiteral("line"), line}});
    }

    sendRequest(
        QStringLiteral("setBreakpoints"),
        {{QStringLiteral("source"), QJsonObject{{QStringLiteral("path"), sourcePath}}},
         {QStringLiteral("breakpoints"), breakpoints}},
        [this, sourcePath](const QJsonObject& body) {
            // The adapter says where each one actually landed, which can differ
            // from where it was clicked: a breakpoint on a blank line moves to
            // the next executable one, and showing it where the user clicked
            // would be a lie.
            std::vector<Breakpoint> verified;

            const QJsonArray items = body.value(QStringLiteral("breakpoints")).toArray();
            verified.reserve(static_cast<size_t>(items.size()));

            for (const QJsonValue& item : items) {
                const QJsonObject object = item.toObject();

                Breakpoint breakpoint;
                breakpoint.sourcePath = sourcePath;
                breakpoint.verified = object.value(QStringLiteral("verified")).toBool();
                breakpoint.actualLine = object.value(QStringLiteral("line")).toInt();
                breakpoint.line = breakpoint.actualLine;
                verified.push_back(breakpoint);
            }

            emit breakpointsVerified(sourcePath, verified);
        });
}

} // namespace keys::debugger
