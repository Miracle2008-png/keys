#include "langsvc/LanguageServiceManager.h"

#include "core/Log.h"

#include <algorithm>

namespace keys::langsvc {

LanguageServiceManager::LanguageServiceManager(QObject* parent)
    : QObject(parent), m_configs(defaultServers())
{
}

LanguageServiceManager::~LanguageServiceManager() = default;

std::vector<ServerConfig> LanguageServiceManager::defaultServers()
{
    const auto server = [](const char* name, const char* program,
                           const QStringList& arguments, const QStringList& languages) {
        ServerConfig config;
        config.name = QString::fromLatin1(name);
        config.program = QString::fromLatin1(program);
        config.arguments = arguments;
        config.languageIds = languages;
        return config;
    };

    // Each is the conventional server for its language, invoked the way its own
    // documentation says. None is bundled: they are separate installs, and one
    // that is missing simply never starts.
    return {
        server("clangd", "clangd", {QStringLiteral("--background-index")},
               {QStringLiteral("c"), QStringLiteral("cpp")}),

        server("rust-analyzer", "rust-analyzer", {}, {QStringLiteral("rust")}),

        server("gopls", "gopls", {}, {QStringLiteral("go")}),

        server("pyright", "pyright-langserver", {QStringLiteral("--stdio")},
               {QStringLiteral("python")}),

        server("typescript-language-server", "typescript-language-server",
               {QStringLiteral("--stdio")},
               {QStringLiteral("javascript"), QStringLiteral("javascriptreact"),
                QStringLiteral("typescript"), QStringLiteral("typescriptreact")}),
    };
}

void LanguageServiceManager::setProjectRoot(const QString& root)
{
    if (root == m_root) {
        return;
    }

    // A server's index is built for one root, so nothing survives the change.
    stopAll();

    m_root = root;
    m_failed.clear();   // a different project may have different tools available
}

void LanguageServiceManager::stopAll()
{
    for (auto& [name, client] : m_clients) {
        if (client) {
            client->stop();
        }
    }
    m_clients.clear();
    emit clientStateChanged();
}

QStringList LanguageServiceManager::runningServers() const
{
    QStringList names;
    for (const auto& [name, client] : m_clients) {
        if (client && client->isRunning()) {
            names << name;
        }
    }
    names.sort();
    return names;
}

LanguageClient* LanguageServiceManager::existingClientFor(const QString& path) const
{
    const QString languageId = languageIdForPath(path);
    if (languageId.isEmpty()) {
        return nullptr;
    }

    for (const auto& [name, client] : m_clients) {
        if (client && client->handles(languageId)) {
            return client.get();
        }
    }
    return nullptr;
}

LanguageClient* LanguageServiceManager::clientFor(const QString& path)
{
    if (m_root.isEmpty()) {
        return nullptr;
    }

    const QString languageId = languageIdForPath(path);
    if (languageId.isEmpty()) {
        // A file type Keys has no mapping for. Not an error: a text file has no
        // language server and does not need one.
        return nullptr;
    }
    return ensureClient(languageId);
}

LanguageClient* LanguageServiceManager::ensureClient(const QString& languageId)
{
    // Find the configuration that handles this language.
    const auto configIt =
        std::find_if(m_configs.cbegin(), m_configs.cend(),
                     [&languageId](const ServerConfig& config) {
                         return config.languageIds.contains(languageId);
                     });
    if (configIt == m_configs.cend()) {
        return nullptr;
    }

    // Already tried and failed: not retried, so a missing binary costs one
    // attempt rather than one per file opened.
    if (m_failed.contains(configIt->name)) {
        return nullptr;
    }

    if (const auto it = m_clients.find(configIt->name); it != m_clients.cend()) {
        return it->second.get();
    }

    auto client = std::make_unique<LanguageClient>(*configIt);
    LanguageClient* raw = client.get();
    const QString serverName = configIt->name;

    connect(raw, &LanguageClient::stateChanged,
            this, &LanguageServiceManager::clientStateChanged);
    connect(raw, &LanguageClient::diagnosticsPublished,
            this, &LanguageServiceManager::diagnosticsPublished);
    connect(raw, &LanguageClient::serverMessage,
            this, &LanguageServiceManager::serverMessage);

    connect(raw, &LanguageClient::failed, this,
            [this, serverName](const QString& message) {
                // Remembered so it is not retried per keystroke, and reported
                // once so a missing install is one message rather than a stream.
                m_failed.insert(serverName);
                emit serverFailed(serverName, message);
            });

    if (const core::Status status = client->start(m_root); !status) {
        m_failed.insert(serverName);
        emit serverFailed(serverName, status.error().message());
        return nullptr;
    }

    m_clients.emplace(configIt->name, std::move(client));
    emit clientStateChanged();
    return raw;
}

} // namespace keys::langsvc
