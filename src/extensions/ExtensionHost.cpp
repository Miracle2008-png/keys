#include "extensions/ExtensionHost.h"

#include "core/Log.h"

#include <QDir>
#include <QJsonArray>
#include <QProcess>

using keys::langsvc::JsonRpcCodec;
using keys::langsvc::RpcMessage;

namespace keys::extensions {
namespace {

/// JSON-RPC's own code for a method the peer refuses. Reusing it means an
/// extension written against any JSON-RPC library already understands it.
constexpr int kMethodNotFound = -32601;

/// Refused for want of a capability. Outside JSON-RPC's reserved range, so it
/// cannot be mistaken for a protocol error.
constexpr int kCapabilityDenied = -32000;

} // namespace

ExtensionHost::ExtensionHost(Manifest manifest, QString directory, QObject* parent)
    : QObject(parent), m_manifest(std::move(manifest)), m_directory(std::move(directory))
{
}

ExtensionHost::~ExtensionHost()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // Killed rather than asked politely: this may be application shutdown,
        // and an extension that hangs must not hold up the quit. It is a
        // separate process, so nothing of ours is lost.
        m_process->kill();
        m_process->waitForFinished(kShutdownGraceMs);
    }
}

void ExtensionHost::setState(State state)
{
    if (state == m_state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

bool ExtensionHost::hasCapability(Capability capability) const
{
    // Both declared and granted. A grant for something never declared would be
    // a bug on our side, and honouring it would let a manifest edit widen an
    // existing grant silently.
    return m_manifest.declares(capability) && m_granted.contains(capabilityId(capability));
}

core::Status ExtensionHost::start(const QSet<QString>& granted, const QString& workspaceRoot)
{
    if (m_state == State::Running || m_state == State::Starting) {
        return core::Err(core::ErrorCode::AlreadyExists,
                         QStringLiteral("The extension is already running"), m_manifest.id);
    }

    m_granted = granted;
    m_workspaceRoot = workspaceRoot;
    m_codec.reset();

    // The entry program is either a bare name resolved on PATH - an interpreter
    // like `python` or `node`, which is how a scripted extension declares what
    // runs it - or a file inside the extension's own directory.
    //
    // A relative path that escapes the directory is refused: a manifest naming
    // "../../../bin/sh" would otherwise run something the user never installed.
    // A bare name cannot escape anything, because it names no path at all.
    const QDir directory(m_directory);
    const bool isBareName = !m_manifest.entryPoint.contains(QLatin1Char('/'))
                            && !m_manifest.entryPoint.contains(QLatin1Char('\\'));

    QString program = m_manifest.entryPoint;

    if (!isBareName) {
        program = QDir::cleanPath(directory.absoluteFilePath(m_manifest.entryPoint));

        const QString base = QDir::cleanPath(directory.absolutePath());
        if (!program.startsWith(base + QLatin1Char('/')) && program != base) {
            return core::Err(core::ErrorCode::PermissionDenied,
                             QStringLiteral("The entry program must be inside the "
                                            "extension's own directory"),
                             m_manifest.id);
        }
    }

    m_process = std::make_unique<QProcess>();
    m_process->setWorkingDirectory(m_directory);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process.get(), &QProcess::readyReadStandardOutput,
            this, &ExtensionHost::onReadyRead);

    connect(m_process.get(), &QProcess::readyReadStandardError, this, [this] {
        const QString text = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (!text.isEmpty()) {
            // The extension's own log. Surfaced rather than discarded: an
            // extension that is failing usually explains itself here.
            emit logged(text);
        }
    });

    connect(m_process.get(), &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart) {
                    return;
                }
                setState(State::Failed);
                emit failed(tr("Could not start the extension '%1'.").arg(m_manifest.name));
            });

    connect(m_process.get(), &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
                const bool wasRunning = m_state == State::Running;
                setState(State::Stopped);

                // A crash is reported; a clean exit is not. An extension may
                // legitimately finish its work and stop.
                if (wasRunning && (status == QProcess::CrashExit || exitCode != 0)) {
                    emit failed(tr("The extension '%1' stopped unexpectedly.")
                                    .arg(m_manifest.name));
                }
            });

    setState(State::Starting);
    m_process->start(program, m_manifest.entryArguments);

    // The extension is told what it was granted, so it can decide what to offer
    // rather than discovering its limits by being refused.
    QJsonArray capabilities;
    for (const QString& capability : m_granted) {
        capabilities.append(capability);
    }

    sendNotification(QStringLiteral("keys/initialize"),
                     {{QStringLiteral("workspaceRoot"), workspaceRoot},
                      {QStringLiteral("extensionId"), m_manifest.id},
                      {QStringLiteral("capabilities"), capabilities}});

    setState(State::Running);
    return core::Ok();
}

void ExtensionHost::stop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        setState(State::Stopped);
        return;
    }

    // Asked to shut down, then terminated, then killed. An extension holding a
    // file open deserves the chance to close it.
    sendNotification(QStringLiteral("keys/shutdown"), {});

    if (!m_process->waitForFinished(kShutdownGraceMs)) {
        m_process->terminate();
        if (!m_process->waitForFinished(kShutdownGraceMs)) {
            qCWarning(lcCore) << m_manifest.id << "ignored shutdown; killing";
            m_process->kill();
            m_process->waitForFinished(kShutdownGraceMs);
        }
    }
    setState(State::Stopped);
}

void ExtensionHost::send(const QJsonObject& message)
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }

    QJsonObject full = message;
    full.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    m_process->write(JsonRpcCodec::encode(full));
}

void ExtensionHost::sendNotification(const QString& method, const QJsonObject& params)
{
    send({{QStringLiteral("method"), method}, {QStringLiteral("params"), params}});
}

void ExtensionHost::respond(const QJsonValue& id, const QJsonValue& result)
{
    send({{QStringLiteral("id"), id}, {QStringLiteral("result"), result}});
}

void ExtensionHost::respondError(const QJsonValue& id, int code, const QString& message)
{
    send({{QStringLiteral("id"), id},
          {QStringLiteral("error"),
           QJsonObject{{QStringLiteral("code"), code},
                       {QStringLiteral("message"), message}}}});
}

void ExtensionHost::invokeCommand(const QString& commandId)
{
    if (!isRunning()) {
        return;
    }
    sendNotification(QStringLiteral("keys/executeCommand"),
                     {{QStringLiteral("command"), commandId}});
}

void ExtensionHost::onReadyRead()
{
    m_codec.append(m_process->readAllStandardOutput());

    while (const std::optional<RpcMessage> message = m_codec.next()) {
        handleMessage(*message);
    }
}

void ExtensionHost::handleMessage(const RpcMessage& message)
{
    if (message.isRequest()) {
        handleRequest(message);
        return;
    }

    if (message.isNotification()) {
        // Notifications the host acts on. Each still needs its capability: a
        // notification is not a way around the check.
        if (message.method == QLatin1String("keys/log")) {
            emit logged(message.params.value(QStringLiteral("message")).toString());
        }
        return;
    }
}

std::optional<Capability> ExtensionHost::requiredCapability(const QString& method)
{
    // The mapping from method to capability, in one place. A method missing from
    // here needs none, which is why the table is explicit rather than defaulting
    // to "allowed" per call site.
    static const QHash<QString, Capability> required = {
        {QStringLiteral("workspace/readFile"), Capability::ReadWorkspace},
        {QStringLiteral("workspace/listFiles"), Capability::ReadWorkspace},
        {QStringLiteral("workspace/writeFile"), Capability::WriteWorkspace},
        {QStringLiteral("workspace/deleteFile"), Capability::WriteWorkspace},
        {QStringLiteral("editor/getText"), Capability::ReadEditor},
        {QStringLiteral("editor/getSelection"), Capability::ReadEditor},
        {QStringLiteral("editor/insertText"), Capability::ModifyEditor},
        {QStringLiteral("editor/replaceRange"), Capability::ModifyEditor},
        {QStringLiteral("window/showMessage"), Capability::ShowUi},
        {QStringLiteral("process/run"), Capability::RunProcesses},
    };

    const auto it = required.constFind(method);
    return it == required.cend() ? std::nullopt : std::optional<Capability>(*it);
}

void ExtensionHost::handleRequest(const RpcMessage& message)
{
    const QString method = message.method;

    // Capability first, before the method is even looked at. Checking after
    // dispatch is how a permission gets missed on a new method.
    if (const std::optional<Capability> capability = requiredCapability(method)) {
        if (!hasCapability(*capability)) {
            const QString id = capabilityId(*capability);

            // Surfaced rather than silently refused: a user should be able to
            // see that an extension is reaching for something it was not given.
            emit capabilityDenied(method, id);

            respondError(message.id, kCapabilityDenied,
                         QStringLiteral("The '%1' capability was not granted").arg(id));
            return;
        }
    }

    if (method == QLatin1String("window/showMessage")) {
        emit showMessageRequested(message.params.value(QStringLiteral("message")).toString());
        respond(message.id, QJsonValue::Null);
        return;
    }

    // Everything else is refused explicitly. An unimplemented method must
    // answer, or the extension waits forever for a reply that never comes.
    respondError(message.id, kMethodNotFound,
                 QStringLiteral("Keys does not implement '%1'").arg(method));
}

} // namespace keys::extensions
