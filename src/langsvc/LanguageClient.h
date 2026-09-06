#pragma once

#include "core/Result.h"
#include "langsvc/JsonRpc.h"
#include "langsvc/LspTypes.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <vector>

class QProcess;

namespace keys::langsvc {

/// How a language server is launched.
struct ServerConfig {
    QString name;        ///< "clangd", for logs and the status bar
    QString program;
    QStringList arguments;

    /// The language ids this server handles. A document whose id is not here is
    /// never sent to it.
    QStringList languageIds;

    [[nodiscard]] bool isValid() const { return !program.isEmpty() && !languageIds.isEmpty(); }
};

/// One language server, spoken to over stdio.
///
/// **Lifecycle is the hard part, not the messages.** A server must be
/// initialized before anything else is sent, told about every open document,
/// kept in sync with every edit, and shut down politely. Getting that wrong
/// produces a server that silently answers nothing — so the state machine is
/// explicit here and requests made too early are refused rather than dropped.
///
/// **Capabilities decide what is offered.** A server announces what it supports
/// during initialize. Keys asks for nothing it was not offered, which is what
/// stops a missing feature looking like a broken one.
///
/// **Nothing blocks.** Requests are asynchronous with a callback; the process
/// runs with its own pipes on the UI thread's event loop, which is cheap because
/// the work happens in the server's process, not ours.
class LanguageClient : public QObject {
    Q_OBJECT

public:
    enum class State {
        Stopped,
        Starting,      ///< process launched, initialize sent
        Running,       ///< initialized; requests are accepted
        ShuttingDown,
        Failed,
    };

    LanguageClient(ServerConfig config, QObject* parent = nullptr);
    ~LanguageClient() override;

    /// Starts the server for `rootPath`. Fails if the program cannot be found,
    /// which is the common case — a language server is a separate install.
    core::Status start(const QString& rootPath);

    /// Sends shutdown/exit and waits briefly. A server killed without this can
    /// leave index files half-written.
    void stop();

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] bool isRunning() const { return m_state == State::Running; }
    [[nodiscard]] const ServerConfig& config() const { return m_config; }

    /// Whether this server handles a language.
    [[nodiscard]] bool handles(const QString& languageId) const;

    // ---- Document synchronisation -----------------------------------------
    //
    // The server keeps its own copy of every open document. These keep the two
    // identical; a missed edit makes every later answer wrong in ways that look
    // like the server being broken.

    void openDocument(const QString& path, const QString& languageId, const QString& text);
    void closeDocument(const QString& path);

    /// Reports an edit. Incremental when the server supports it, which matters:
    /// resending a 10,000-line file on every keystroke is the difference between
    /// a client that keeps up and one that does not.
    void changeDocument(const QString& path, const LspRange& range,
                        const QString& newText, const QString& fullText);

    void saveDocument(const QString& path);

    // ---- Requests ----------------------------------------------------------

    using CompletionHandler = std::function<void(std::vector<CompletionItem>)>;
    using DefinitionHandler = std::function<void(std::vector<Location>)>;
    using RenameHandler = std::function<void(WorkspaceEdit)>;
    using HoverHandler = std::function<void(QString)>;

    void requestCompletion(const QString& path, const LspPosition& position,
                           CompletionHandler handler);
    void requestDefinition(const QString& path, const LspPosition& position,
                           DefinitionHandler handler);
    void requestHover(const QString& path, const LspPosition& position,
                      HoverHandler handler);

    /// Asks the server to rename the symbol at `position` everywhere.
    ///
    /// The reply is edits across however many files use it, which is exactly
    /// why this belongs to the language server: finding them by text search
    /// would rename comments, strings and unrelated identifiers that happen to
    /// share a name.
    void requestRename(const QString& path, const LspPosition& position,
                       const QString& newName, RenameHandler handler);

    /// What the server said it supports. Consulted before offering a feature.
    [[nodiscard]] bool supportsCompletion() const { return m_supportsCompletion; }
    [[nodiscard]] bool supportsDefinition() const { return m_supportsDefinition; }
    [[nodiscard]] bool supportsRename() const { return m_supportsRename; }
    [[nodiscard]] bool supportsHover() const { return m_supportsHover; }
    [[nodiscard]] bool supportsIncrementalSync() const { return m_incrementalSync; }

signals:
    void stateChanged();

    /// Diagnostics for a file, replacing whatever it had. LSP publishes the
    /// whole set per file, so this is a replacement rather than an addition.
    void diagnosticsPublished(const QString& path, const std::vector<Diagnostic>& diagnostics);

    /// The server said something a person should see, or died.
    void serverMessage(const QString& message);
    void failed(const QString& message);

private:
    void send(const QJsonObject& message);
    void sendNotification(const QString& method, const QJsonObject& params);

    /// Sends a request and remembers who to call with the answer.
    void sendRequest(const QString& method, const QJsonObject& params,
                     std::function<void(const QJsonValue&)> handler);

    void onReadyRead();
    void handleMessage(const RpcMessage& message);
    void handleServerRequest(const RpcMessage& message);

    void sendInitialize(const QString& rootPath);
    void onInitialized(const QJsonValue& result);

    void setState(State state);

    /// The version a document is at. LSP requires a monotonically increasing
    /// number per document so the server can detect a missed change.
    [[nodiscard]] int nextVersion(const QString& path);

    /// Sends the didOpen for every document that was opened while the server was
    /// still starting.
    void flushPendingOpens();

    ServerConfig m_config;
    std::unique_ptr<QProcess> m_process;
    JsonRpcCodec m_codec;

    State m_state = State::Stopped;
    QString m_rootPath;

    int m_nextRequestId = 1;
    QHash<int, std::function<void(const QJsonValue&)>> m_pending;

    /// Open documents and their current version.
    QHash<QString, int> m_documentVersions;

    /// Documents opened before initialize completed.
    ///
    /// A server takes a moment to start, and the file that triggered the start
    /// is opened immediately after - so without this the very first document is
    /// never announced, and every request against it fails with "non-added
    /// document" while the editor looks like the server is broken.
    struct PendingOpen {
        QString path;
        QString languageId;
        QString text;
    };
    std::vector<PendingOpen> m_pendingOpens;

    bool m_supportsCompletion = false;
    bool m_supportsDefinition = false;
    bool m_supportsHover = false;
    bool m_supportsRename = false;
    bool m_incrementalSync = false;

    /// How long to wait for a polite shutdown before killing the process.
    static constexpr int kShutdownGraceMs = 2000;
};

} // namespace keys::langsvc
