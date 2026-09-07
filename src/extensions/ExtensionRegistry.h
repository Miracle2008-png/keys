#pragma once

#include "config/Settings.h"
#include "core/CommandRegistry.h"
#include "extensions/ExtensionHost.h"

#include <QObject>
#include <QString>

#include <map>
#include <memory>

namespace keys::extensions {

/// One installed extension, whether or not it is running.
struct InstalledExtension {
    Manifest manifest;
    QString directory;

    /// Why it could not be loaded, when it could not. Kept so the UI can say
    /// what is wrong with an extension rather than omitting it silently.
    QString error;

    [[nodiscard]] bool isLoadable() const { return error.isEmpty(); }
};

/// Discovers extensions, records what the user granted, and runs them.
///
/// **Grants are per extension and persist.** Approving a capability once should
/// not have to be repeated every launch, and revoking it must actually stop the
/// extension using it — so grants live in settings and are re-applied on start.
///
/// **A broken extension is visible, not hidden.** A manifest that will not parse
/// produces an entry with an error rather than nothing at all; an extension the
/// user installed and cannot see is worse than one that says why it failed.
class ExtensionRegistry : public QObject {
    Q_OBJECT

public:
    ExtensionRegistry(core::CommandRegistry& commands, config::Settings& settings,
                      QObject* parent = nullptr);
    ~ExtensionRegistry() override;

    /// Where extensions are installed: %LOCALAPPDATA%/Keys/extensions or the
    /// platform equivalent.
    [[nodiscard]] static QString extensionsDirectory();

    /// Re-reads the extensions directory. Cheap enough to call when the user
    /// asks; nothing polls.
    void discover();

    /// Installs an extension from a folder containing `keys-extension.json`.
    ///
    /// **Validated before it is copied, not after.** A manifest that will not
    /// parse, or an entry point that is not there, fails without leaving a
    /// half-installed directory behind - which would then appear in the panel
    /// as a broken extension the user did not ask for.
    ///
    /// **Reinstalling replaces.** Installing an id that already exists is an
    /// upgrade, so the old directory is removed first rather than merged with
    /// the new one: a file the new version dropped would otherwise survive.
    ///
    /// Grants are deliberately *not* carried across an upgrade. A new version
    /// can ask for capabilities the old one did not, and silently keeping a
    /// previous yes would grant them without anyone being asked.
    core::Result<QString> installFromDirectory(const QString& sourceDirectory);

    /// Removes an extension and everything it was granted.
    ///
    /// Stops it first: deleting the directory of a running host would leave a
    /// process alive with no files, which fails in ways that are hard to read.
    core::Status uninstall(const QString& extensionId);

    [[nodiscard]] const std::vector<InstalledExtension>& installed() const
    {
        return m_installed;
    }

    /// Starts every extension whose capabilities are all granted. Called when a
    /// project opens, since an extension without a workspace has nothing to act
    /// on.
    void activateAll(const QString& workspaceRoot);
    void deactivateAll();

    [[nodiscard]] bool isRunning(const QString& extensionId) const;

    /// What the user has approved for an extension.
    [[nodiscard]] QSet<QString> grantedCapabilities(const QString& extensionId) const;

    /// Records a grant and starts or stops the extension to match. Revoking is
    /// what makes the grant meaningful: it stops the extension rather than
    /// leaving it running with a capability the user just withdrew.
    void setGranted(const QString& extensionId, const QSet<QString>& capabilities);

    /// Whether every capability an extension declared has been granted.
    [[nodiscard]] bool isFullyGranted(const QString& extensionId) const;

signals:
    void extensionsChanged();
    void runningChanged();

    /// An extension asked to show a message, or something went wrong with one.
    void notice(const QString& message);

private:
    void startExtension(const InstalledExtension& extension, const QString& workspaceRoot);
    void stopExtension(const QString& extensionId);

    /// Registers an extension's contributed commands so they appear in the
    /// palette. Removed again when it stops, or the palette would offer commands
    /// nothing can run.
    void registerContributions(const InstalledExtension& extension);
    void unregisterContributions(const QString& extensionId);

    [[nodiscard]] const InstalledExtension* find(const QString& extensionId) const;

    /// The settings key holding an extension's grants.
    [[nodiscard]] static QString grantKey(const QString& extensionId);

    /// Declares that key if it is not already. The schema cannot know extension
    /// ids ahead of time, so this happens on discovery rather than at startup.
    void ensureGrantKey(const QString& extensionId) const;

    core::CommandRegistry& m_commands;
    config::Settings& m_settings;

    std::vector<InstalledExtension> m_installed;

    /// Running hosts, by extension id.
    std::map<QString, std::unique_ptr<ExtensionHost>> m_hosts;

    /// Command ids registered per extension, so they can be withdrawn exactly.
    QHash<QString, QStringList> m_registeredCommands;

    QString m_workspaceRoot;
};

} // namespace keys::extensions
