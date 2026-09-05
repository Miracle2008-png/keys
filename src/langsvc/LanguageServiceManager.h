#pragma once

#include "langsvc/LanguageClient.h"

#include <QHash>
#include <QSet>
#include <QObject>
#include <QString>

#include <map>
#include <memory>
#include <vector>

namespace keys::langsvc {

/// Owns the language servers and routes documents to them.
///
/// **Servers start on demand.** Launching every configured server when a project
/// opens would cost seconds and memory for languages the user never touches. A
/// server starts when the first file it handles is opened, and the ones that are
/// not installed simply never start — which is the normal case, since a language
/// server is a separate install.
///
/// **Missing is not broken.** A project with no server for its language gets no
/// completions and no diagnostics, and says so once rather than failing per
/// keystroke.
class LanguageServiceManager : public QObject {
    Q_OBJECT

public:
    explicit LanguageServiceManager(QObject* parent = nullptr);
    ~LanguageServiceManager() override;

    /// The servers Keys knows how to launch. Configuration, not code: adding a
    /// language is a row here plus the binary on the user's PATH.
    [[nodiscard]] static std::vector<ServerConfig> defaultServers();

    /// Points the manager at a project. Stops everything running for the
    /// previous one, since a server's index is per project root.
    void setProjectRoot(const QString& root);

    [[nodiscard]] QString projectRoot() const { return m_root; }

    /// The client that handles `path`, starting it if needed. Null when no
    /// server is configured for that language or the project is closed.
    [[nodiscard]] LanguageClient* clientFor(const QString& path);

    /// The client already serving `path`, without starting one.
    [[nodiscard]] LanguageClient* existingClientFor(const QString& path) const;

    void stopAll();

    /// Names of servers currently running, for the status bar.
    [[nodiscard]] QStringList runningServers() const;

signals:
    void clientStateChanged();

    /// Diagnostics arrived for a file, from whichever server owns it.
    void diagnosticsPublished(const QString& path, const std::vector<Diagnostic>& diagnostics);

    /// A server could not start or died. Reported once per server rather than
    /// per request, so a missing toolchain is one message and not a stream.
    void serverFailed(const QString& serverName, const QString& message);

    void serverMessage(const QString& message);

private:
    /// Starts the server for `languageId`, or returns the running one.
    [[nodiscard]] LanguageClient* ensureClient(const QString& languageId);

    QString m_root;
    std::vector<ServerConfig> m_configs;

    /// Keyed by server name, not language: one server can handle several
    /// languages (clangd serves both c and cpp) and must not be started twice.
    ///
    /// std::map rather than QHash: QHash requires its value type to be
    /// copy-constructible, which unique_ptr is not.
    std::map<QString, std::unique_ptr<LanguageClient>> m_clients;

    /// Servers that failed to start, so Keys does not retry on every keystroke
    /// and does not repeat the message.
    QSet<QString> m_failed;
};

} // namespace keys::langsvc
