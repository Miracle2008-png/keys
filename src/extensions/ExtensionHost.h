#pragma once

#include "core/Result.h"
#include "extensions/Manifest.h"
#include "langsvc/JsonRpc.h"

#include <QHash>
#include <QObject>
#include <QSet>

#include <functional>
#include <memory>

class QProcess;

namespace keys::extensions {

/// One running extension, in its own process.
///
/// **The process boundary is the security boundary.** An extension never loads
/// as in-process code, so it cannot crash Keys, cannot read its memory, and
/// cannot reach anything it was not granted. That costs IPC latency, and the
/// cost is the point: an in-process model is unfixable once extensions exist.
///
/// **Every request is checked against the grant.** The host refuses anything
/// outside the capabilities the user approved, and says so — an extension that
/// asked for what it needed gets a clear answer rather than silence.
class ExtensionHost : public QObject {
    Q_OBJECT

public:
    enum class State { Stopped, Starting, Running, Failed };

    ExtensionHost(Manifest manifest, QString directory, QObject* parent = nullptr);
    ~ExtensionHost() override;

    /// Starts the extension process. `granted` is what the user approved, which
    /// can be less than the manifest asked for.
    core::Status start(const QSet<QString>& granted, const QString& workspaceRoot);
    void stop();

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] bool isRunning() const { return m_state == State::Running; }
    [[nodiscard]] const Manifest& manifest() const { return m_manifest; }

    /// Whether a capability was both declared and granted. Both, because a
    /// grant for something never declared would be a bug on our side.
    [[nodiscard]] bool hasCapability(Capability capability) const;

    /// Asks the extension to run one of its contributed commands.
    void invokeCommand(const QString& commandId);

signals:
    void stateChanged();

    /// The extension asked Keys to do something. The host has already checked
    /// the capability, so the workbench can act without re-checking.
    void showMessageRequested(const QString& message);

    /// A request the extension was not granted. Surfaced so the user can see
    /// what an extension is trying to do, rather than it failing invisibly.
    void capabilityDenied(const QString& method, const QString& capability);

    void failed(const QString& message);
    void logged(const QString& text);

private:
    void send(const QJsonObject& message);
    void sendNotification(const QString& method, const QJsonObject& params);
    void respond(const QJsonValue& id, const QJsonValue& result);
    void respondError(const QJsonValue& id, int code, const QString& message);

    void onReadyRead();
    void handleMessage(const langsvc::RpcMessage& message);

    /// Handles one request from the extension, after checking its capability.
    void handleRequest(const langsvc::RpcMessage& message);

    /// The capability a method needs, or nothing when it needs none.
    [[nodiscard]] static std::optional<Capability> requiredCapability(const QString& method);

    void setState(State state);

    Manifest m_manifest;
    QString m_directory;
    QString m_workspaceRoot;

    std::unique_ptr<QProcess> m_process;
    langsvc::JsonRpcCodec m_codec;

    State m_state = State::Stopped;

    /// The capabilities actually in force: declared and granted.
    QSet<QString> m_granted;

    int m_nextRequestId = 1;

    static constexpr int kShutdownGraceMs = 2000;

    /// An extension that will not exit is killed. It is a separate process, so
    /// this costs nothing but the extension's own state.
    static constexpr int kStartupTimeoutMs = 10000;
};

} // namespace keys::extensions
