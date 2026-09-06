#include "langsvc/LanguageClient.h"

#include "core/Log.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QProcess>

namespace keys::langsvc {
namespace {

/// LSP's TextDocumentSyncKind. Full resends the whole file, Incremental sends
/// only the changed range.
constexpr int kSyncFull = 1;
constexpr int kSyncIncremental = 2;

} // namespace

LanguageClient::LanguageClient(ServerConfig config, QObject* parent)
    : QObject(parent), m_config(std::move(config))
{
}

LanguageClient::~LanguageClient()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // Not a polite shutdown: the destructor may be running during
        // application exit, where waiting on a server that has stopped
        // responding would hang the quit.
        m_process->kill();
        m_process->waitForFinished(kShutdownGraceMs);
    }
}

bool LanguageClient::handles(const QString& languageId) const
{
    return m_config.languageIds.contains(languageId);
}

void LanguageClient::setState(State state)
{
    if (state == m_state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

core::Status LanguageClient::start(const QString& rootPath)
{
    if (m_state != State::Stopped && m_state != State::Failed) {
        return core::Err(core::ErrorCode::AlreadyExists,
                         QStringLiteral("The server is already running"), m_config.name);
    }
    if (!m_config.isValid()) {
        return core::Err(core::ErrorCode::InvalidArgument,
                         QStringLiteral("The server configuration is incomplete"),
                         m_config.name);
    }

    m_rootPath = rootPath;
    m_codec.reset();
    m_pending.clear();
    m_documentVersions.clear();
    m_pendingOpens.clear();

    m_process = std::make_unique<QProcess>();
    m_process->setWorkingDirectory(rootPath);

    // stderr is the server's own log, not part of the protocol. Kept separate so
    // a chatty server cannot corrupt the message stream on stdout.
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process.get(), &QProcess::readyReadStandardOutput,
            this, &LanguageClient::onReadyRead);

    connect(m_process.get(), &QProcess::readyReadStandardError, this, [this] {
        const QString text = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
        if (!text.isEmpty()) {
            qCDebug(lcCore) << m_config.name << "stderr:" << text;
        }
    });

    connect(m_process.get(), &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart) {
                    return;
                }
                // A language server is a separate install, so this is the most
                // common failure by far. Naming the program is what makes the
                // message actionable.
                const QString message =
                    tr("Could not start the %1 language server. Is '%2' installed "
                       "and on your PATH?")
                        .arg(m_config.name, m_config.program);
                setState(State::Failed);
                emit failed(message);
            });

    connect(m_process.get(), &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
                if (m_state == State::ShuttingDown || m_state == State::Stopped) {
                    setState(State::Stopped);
                    return;
                }
                // Died while we still expected it to be there. Every pending
                // request is now unanswerable, so they are dropped rather than
                // left to leak their callbacks.
                m_pending.clear();
                setState(State::Failed);
                emit failed(tr("The %1 language server stopped unexpectedly "
                               "(exit code %2).")
                                .arg(m_config.name)
                                .arg(status == QProcess::CrashExit ? -1 : exitCode));
            });

    setState(State::Starting);
    m_process->start(m_config.program, m_config.arguments);

    // initialize is sent immediately rather than waiting for started(): QProcess
    // buffers writes until the process is up, so this cannot race.
    sendInitialize(rootPath);
    return core::Ok();
}

void LanguageClient::stop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        setState(State::Stopped);
        return;
    }

    setState(State::ShuttingDown);

    // The protocol's own shutdown: a server that indexes a project writes that
    // index on exit, and killing it can leave a half-written one it will refuse
    // to reuse next time.
    sendRequest(QStringLiteral("shutdown"), {}, [this](const QJsonValue&) {
        sendNotification(QStringLiteral("exit"), {});
    });

    // Bounded: a server that ignores shutdown must not hold up the application.
    if (!m_process->waitForFinished(kShutdownGraceMs)) {
        qCWarning(lcCore) << m_config.name << "ignored shutdown; killing";
        m_process->kill();
        m_process->waitForFinished(kShutdownGraceMs);
    }
    setState(State::Stopped);
}

void LanguageClient::send(const QJsonObject& message)
{
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        return;
    }

    QJsonObject full = message;
    full.insert(QStringLiteral("jsonrpc"), QStringLiteral("2.0"));
    m_process->write(JsonRpcCodec::encode(full));
}

void LanguageClient::sendNotification(const QString& method, const QJsonObject& params)
{
    send({{QStringLiteral("method"), method}, {QStringLiteral("params"), params}});
}

void LanguageClient::sendRequest(const QString& method, const QJsonObject& params,
                                 std::function<void(const QJsonValue&)> handler)
{
    const int id = m_nextRequestId++;
    m_pending.insert(id, std::move(handler));

    send({{QStringLiteral("id"), id},
          {QStringLiteral("method"), method},
          {QStringLiteral("params"), params}});
}

void LanguageClient::onReadyRead()
{
    m_codec.append(m_process->readAllStandardOutput());

    // Repeatedly: one read can carry several messages, and stopping after the
    // first would leave the rest sitting in the buffer until the next read
    // happened to arrive.
    while (const std::optional<RpcMessage> message = m_codec.next()) {
        handleMessage(*message);
    }
}

void LanguageClient::handleMessage(const RpcMessage& message)
{
    if (message.isResponse()) {
        const int id = message.id.toInt(-1);
        const auto handler = m_pending.take(id);

        if (message.hasError) {
            // A failed request is reported and its handler dropped: calling it
            // with an empty result would look like "no completions" rather than
            // "the server could not answer".
            qCWarning(lcCore) << m_config.name << "request" << id << "failed:"
                              << message.error.value(QStringLiteral("message")).toString();
            return;
        }
        if (handler) {
            handler(message.result);
        }
        return;
    }

    if (message.isRequest()) {
        handleServerRequest(message);
        return;
    }

    // Notifications.
    if (message.method == QLatin1String("textDocument/publishDiagnostics")) {
        const QString path = uriToPath(message.params.value(QStringLiteral("uri")).toString());

        std::vector<Diagnostic> diagnostics;
        const QJsonArray items = message.params.value(QStringLiteral("diagnostics")).toArray();
        diagnostics.reserve(static_cast<size_t>(items.size()));
        for (const QJsonValue& item : items) {
            diagnostics.push_back(Diagnostic::fromJson(item.toObject()));
        }

        emit diagnosticsPublished(path, diagnostics);
        return;
    }

    if (message.method == QLatin1String("window/showMessage")
        || message.method == QLatin1String("window/logMessage")) {
        const QString text = message.params.value(QStringLiteral("message")).toString();
        if (message.method.endsWith(QLatin1String("showMessage"))) {
            emit serverMessage(text);   // meant for a person
        } else {
            qCDebug(lcCore) << m_config.name << text;
        }
    }
}

void LanguageClient::handleServerRequest(const RpcMessage& message)
{
    // A server request that goes unanswered blocks the server: several of them
    // wait for a reply before continuing to index. So everything gets an answer,
    // even if that answer is "not supported".
    QJsonObject response{{QStringLiteral("id"), message.id}};

    if (message.method == QLatin1String("workspace/configuration")) {
        // One null per requested item: Keys has no per-server settings yet, and
        // null means "use your default", which is what we want.
        const QJsonArray items =
            message.params.value(QStringLiteral("items")).toArray();
        QJsonArray result;
        for (int i = 0; i < items.size(); ++i) {
            result.append(QJsonValue::Null);
        }
        response.insert(QStringLiteral("result"), result);

    } else if (message.method == QLatin1String("client/registerCapability")
               || message.method == QLatin1String("client/unregisterCapability")) {
        // Accepted so the server proceeds. Dynamic registration is not acted on
        // yet; the static capabilities from initialize are what Keys uses.
        response.insert(QStringLiteral("result"), QJsonValue::Null);

    } else if (message.method == QLatin1String("window/workDoneProgress/create")) {
        response.insert(QStringLiteral("result"), QJsonValue::Null);

    } else {
        // MethodNotFound. Honest, and it keeps the server moving.
        response.insert(QStringLiteral("error"),
                        QJsonObject{{QStringLiteral("code"), -32601},
                                    {QStringLiteral("message"),
                                     QStringLiteral("Not supported by Keys")}});
    }
    send(response);
}

void LanguageClient::sendInitialize(const QString& rootPath)
{
    // Only capabilities Keys actually implements are declared. Claiming one it
    // does not have makes a server send messages nothing handles, which looks
    // like the server misbehaving.
    const QJsonObject textDocument{
        {QStringLiteral("synchronization"),
         QJsonObject{{QStringLiteral("dynamicRegistration"), false},
                     {QStringLiteral("didSave"), true}}},
        {QStringLiteral("completion"),
         QJsonObject{{QStringLiteral("dynamicRegistration"), false},
                     {QStringLiteral("completionItem"),
                      QJsonObject{{QStringLiteral("snippetSupport"), false}}}}},
        {QStringLiteral("definition"),
         QJsonObject{{QStringLiteral("dynamicRegistration"), false}}},
        {QStringLiteral("hover"),
         QJsonObject{{QStringLiteral("dynamicRegistration"), false},
                     {QStringLiteral("contentFormat"),
                      QJsonArray{QStringLiteral("plaintext"), QStringLiteral("markdown")}}}},
        {QStringLiteral("publishDiagnostics"),
         QJsonObject{{QStringLiteral("relatedInformation"), false}}},
    };

    const QJsonObject params{
        {QStringLiteral("processId"), static_cast<int>(QCoreApplication::applicationPid())},
        {QStringLiteral("rootUri"), pathToUri(rootPath)},
        {QStringLiteral("capabilities"),
         QJsonObject{{QStringLiteral("textDocument"), textDocument},
                     {QStringLiteral("workspace"),
                      QJsonObject{{QStringLiteral("configuration"), true}}}}},
        {QStringLiteral("clientInfo"),
         QJsonObject{{QStringLiteral("name"), QStringLiteral("Keys")}}},
    };

    sendRequest(QStringLiteral("initialize"), params,
                [this](const QJsonValue& result) { onInitialized(result); });
}

void LanguageClient::onInitialized(const QJsonValue& result)
{
    const QJsonObject capabilities =
        result.toObject().value(QStringLiteral("capabilities")).toObject();

    // presence, not truth: a server advertises completion by sending a
    // completionProvider object, which may be empty.
    m_supportsCompletion = capabilities.contains(QStringLiteral("completionProvider"));

    // These can be a boolean or an options object, so both shapes count.
    const auto supports = [&capabilities](const char* key) {
        const QJsonValue value = capabilities.value(QLatin1String(key));
        return value.isObject() || value.toBool();
    };
    m_supportsDefinition = supports("definitionProvider");
    m_supportsHover = supports("hoverProvider");
    m_supportsRename = supports("renameProvider");

    // Sync kind is a number or an options object holding one.
    const QJsonValue sync = capabilities.value(QStringLiteral("textDocumentSync"));
    const int syncKind =
        sync.isObject() ? sync.toObject().value(QStringLiteral("change")).toInt(kSyncFull)
                        : sync.toInt(kSyncFull);
    m_incrementalSync = syncKind == kSyncIncremental;

    sendNotification(QStringLiteral("initialized"), {});
    setState(State::Running);

    flushPendingOpens();

    qCInfo(lcCore) << m_config.name << "ready:"
                   << "completion" << m_supportsCompletion
                   << "definition" << m_supportsDefinition
                   << "hover" << m_supportsHover
                   << "rename" << m_supportsRename
                   << "incremental" << m_incrementalSync;
}

void LanguageClient::flushPendingOpens()
{
    std::vector<PendingOpen> pending;
    pending.swap(m_pendingOpens);

    for (const PendingOpen& open : pending) {
        openDocument(open.path, open.languageId, open.text);
    }
}

int LanguageClient::nextVersion(const QString& path)
{
    return ++m_documentVersions[path];
}

void LanguageClient::openDocument(const QString& path, const QString& languageId,
                                  const QString& text)
{
    if (!handles(languageId)) {
        return;
    }

    // Held until initialize completes rather than dropped. The document that
    // caused the server to start arrives before it is ready, and losing that one
    // makes every request against it fail with "non-added document".
    if (m_state == State::Starting) {
        m_pendingOpens.push_back({path, languageId, text});
        return;
    }

    if (!isRunning()) {
        return;
    }

    m_documentVersions.insert(path, 1);

    sendNotification(
        QStringLiteral("textDocument/didOpen"),
        {{QStringLiteral("textDocument"),
          QJsonObject{{QStringLiteral("uri"), pathToUri(path)},
                      {QStringLiteral("languageId"), languageId},
                      {QStringLiteral("version"), 1},
                      {QStringLiteral("text"), text}}}});
}

void LanguageClient::closeDocument(const QString& path)
{
    if (!isRunning() || !m_documentVersions.contains(path)) {
        return;
    }
    m_documentVersions.remove(path);

    sendNotification(QStringLiteral("textDocument/didClose"),
                     {{QStringLiteral("textDocument"),
                       QJsonObject{{QStringLiteral("uri"), pathToUri(path)}}}});
}

void LanguageClient::changeDocument(const QString& path, const LspRange& range,
                                    const QString& newText, const QString& fullText)
{
    if (!isRunning() || !m_documentVersions.contains(path)) {
        return;
    }

    QJsonArray changes;
    if (m_incrementalSync) {
        // The range that was replaced plus what replaced it. Resending the whole
        // file on every keystroke is what makes a client fall behind on a large
        // one.
        changes.append(QJsonObject{{QStringLiteral("range"), range.toJson()},
                                   {QStringLiteral("text"), newText}});
    } else {
        changes.append(QJsonObject{{QStringLiteral("text"), fullText}});
    }

    sendNotification(
        QStringLiteral("textDocument/didChange"),
        {{QStringLiteral("textDocument"),
          QJsonObject{{QStringLiteral("uri"), pathToUri(path)},
                      {QStringLiteral("version"), nextVersion(path)}}},
         {QStringLiteral("contentChanges"), changes}});
}

void LanguageClient::saveDocument(const QString& path)
{
    if (!isRunning() || !m_documentVersions.contains(path)) {
        return;
    }
    sendNotification(QStringLiteral("textDocument/didSave"),
                     {{QStringLiteral("textDocument"),
                       QJsonObject{{QStringLiteral("uri"), pathToUri(path)}}}});
}

void LanguageClient::requestCompletion(const QString& path, const LspPosition& position,
                                       CompletionHandler handler)
{
    if (!isRunning() || !m_supportsCompletion) {
        handler({});   // called anyway, so the caller is never left waiting
        return;
    }

    sendRequest(QStringLiteral("textDocument/completion"),
                {{QStringLiteral("textDocument"),
                  QJsonObject{{QStringLiteral("uri"), pathToUri(path)}}},
                 {QStringLiteral("position"), position.toJson()}},
                [handler = std::move(handler)](const QJsonValue& result) {
                    // The response is a plain array or a CompletionList holding
                    // one; both are legal and servers differ.
                    const QJsonArray items =
                        result.isArray()
                            ? result.toArray()
                            : result.toObject().value(QStringLiteral("items")).toArray();

                    std::vector<CompletionItem> completions;
                    completions.reserve(static_cast<size_t>(items.size()));
                    for (const QJsonValue& item : items) {
                        completions.push_back(CompletionItem::fromJson(item.toObject()));
                    }
                    handler(std::move(completions));
                });
}

void LanguageClient::requestDefinition(const QString& path, const LspPosition& position,
                                       DefinitionHandler handler)
{
    if (!isRunning() || !m_supportsDefinition) {
        handler({});
        return;
    }

    sendRequest(QStringLiteral("textDocument/definition"),
                {{QStringLiteral("textDocument"),
                  QJsonObject{{QStringLiteral("uri"), pathToUri(path)}}},
                 {QStringLiteral("position"), position.toJson()}},
                [handler = std::move(handler)](const QJsonValue& result) {
                    // One Location, an array of them, or an array of LocationLink
                    // - all three are legal responses.
                    std::vector<Location> locations;

                    const auto append = [&locations](const QJsonObject& object) {
                        if (object.contains(QStringLiteral("targetUri"))) {
                            // LocationLink: different field names, same meaning.
                            Location location;
                            location.uri = object.value(QStringLiteral("targetUri")).toString();
                            location.range = LspRange::fromJson(
                                object.value(QStringLiteral("targetSelectionRange")).toObject());
                            locations.push_back(std::move(location));
                        } else if (object.contains(QStringLiteral("uri"))) {
                            locations.push_back(Location::fromJson(object));
                        }
                    };

                    if (result.isArray()) {
                        for (const QJsonValue& item : result.toArray()) {
                            append(item.toObject());
                        }
                    } else if (result.isObject()) {
                        append(result.toObject());
                    }
                    handler(std::move(locations));
                });
}

void LanguageClient::requestRename(const QString& path, const LspPosition& position,
                                   const QString& newName, RenameHandler handler)
{
    if (!isRunning() || !m_supportsRename || newName.isEmpty()) {
        handler({});
        return;
    }

    sendRequest(QStringLiteral("textDocument/rename"),
                {{QStringLiteral("textDocument"),
                  QJsonObject{{QStringLiteral("uri"), pathToUri(path)}}},
                 {QStringLiteral("position"), position.toJson()},
                 {QStringLiteral("newName"), newName}},
                [handler = std::move(handler)](const QJsonValue& result) {
                    WorkspaceEdit edit;

                    const QJsonObject object = result.toObject();

                    // `changes` is the simple form: a map of uri to edits.
                    // `documentChanges` is the versioned form, which servers
                    // prefer when they can. Both are legal and clangd sends
                    // the second, so both are read.
                    const QJsonObject changes =
                        object.value(QStringLiteral("changes")).toObject();
                    for (auto it = changes.begin(); it != changes.end(); ++it) {
                        std::vector<TextEdit> edits;
                        const QJsonArray array = it.value().toArray();
                        edits.reserve(static_cast<size_t>(array.size()));
                        for (const QJsonValue& value : array) {
                            edits.push_back(TextEdit::fromJson(value.toObject()));
                        }
                        edit.changes.insert(uriToPath(it.key()), std::move(edits));
                    }

                    const QJsonArray documentChanges =
                        object.value(QStringLiteral("documentChanges")).toArray();
                    for (const QJsonValue& value : documentChanges) {
                        const QJsonObject change = value.toObject();
                        const QString uri = change.value(QStringLiteral("textDocument"))
                                                .toObject()
                                                .value(QStringLiteral("uri"))
                                                .toString();
                        if (uri.isEmpty()) {
                            continue;   // a create/rename/delete operation, not an edit
                        }

                        std::vector<TextEdit> edits;
                        const QJsonArray array =
                            change.value(QStringLiteral("edits")).toArray();
                        edits.reserve(static_cast<size_t>(array.size()));
                        for (const QJsonValue& item : array) {
                            edits.push_back(TextEdit::fromJson(item.toObject()));
                        }
                        edit.changes.insert(uriToPath(uri), std::move(edits));
                    }

                    handler(std::move(edit));
                });
}

void LanguageClient::requestHover(const QString& path, const LspPosition& position,
                                  HoverHandler handler)
{
    if (!isRunning() || !m_supportsHover) {
        handler({});
        return;
    }

    sendRequest(QStringLiteral("textDocument/hover"),
                {{QStringLiteral("textDocument"),
                  QJsonObject{{QStringLiteral("uri"), pathToUri(path)}}},
                 {QStringLiteral("position"), position.toJson()}},
                [handler = std::move(handler)](const QJsonValue& result) {
                    const QJsonValue contents =
                        result.toObject().value(QStringLiteral("contents"));

                    // MarkupContent, a MarkedString, or an array of them.
                    QStringList parts;
                    const auto appendValue = [&parts](const QJsonValue& value) {
                        if (value.isString()) {
                            parts << value.toString();
                        } else if (value.isObject()) {
                            parts << value.toObject().value(QStringLiteral("value")).toString();
                        }
                    };

                    if (contents.isArray()) {
                        for (const QJsonValue& item : contents.toArray()) {
                            appendValue(item);
                        }
                    } else {
                        appendValue(contents);
                    }

                    handler(parts.join(QStringLiteral("\n\n")).trimmed());
                });
}

} // namespace keys::langsvc
