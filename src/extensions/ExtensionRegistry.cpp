#include "extensions/ExtensionRegistry.h"

#include "core/Log.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QStandardPaths>

#include <algorithm>

namespace keys::extensions {
namespace {

/// Grants are stored as a comma-separated list under one key per extension.
/// A key per capability would be tidier and would also mean the schema had to
/// know every extension's id in advance, which it cannot.
constexpr auto kGrantPrefix = "extensions.grants.";

} // namespace

ExtensionRegistry::ExtensionRegistry(core::CommandRegistry& commands,
                                     config::Settings& settings, QObject* parent)
    : QObject(parent), m_commands(commands), m_settings(settings)
{
    discover();
}

ExtensionRegistry::~ExtensionRegistry()
{
    // Hosts are stopped explicitly rather than left to their destructors, so a
    // shutdown notification reaches each extension before the process dies.
    deactivateAll();
}

QString ExtensionRegistry::extensionsDirectory()
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(base).filePath(QStringLiteral("extensions"));
}

QString ExtensionRegistry::grantKey(const QString& extensionId)
{
    return QLatin1String(kGrantPrefix) + extensionId;
}

void ExtensionRegistry::discover()
{
    m_installed.clear();

    const QDir root(extensionsDirectory());
    if (!root.exists()) {
        // No extensions directory is the normal state, not an error.
        emit extensionsChanged();
        return;
    }

    const QStringList entries =
        root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString& entry : entries) {
        const QString directory = root.filePath(entry);

        InstalledExtension extension;
        extension.directory = directory;

        const core::Result<Manifest> manifest = Manifest::load(directory);
        if (manifest) {
            extension.manifest = manifest.value();
        } else {
            // Kept with its error rather than dropped: an extension the user
            // installed and cannot see is worse than one that says why it
            // failed.
            extension.error = manifest.error().message();
            extension.manifest.id = entry;
            extension.manifest.name = entry;
            qCWarning(lcCore) << "extension" << entry << "failed to load:"
                              << extension.error;
        }

        ensureGrantKey(extension.manifest.id);
        m_installed.push_back(std::move(extension));
    }

    emit extensionsChanged();
}

namespace {

/// Copies a directory tree. Qt has no recursive copy, and shelling out to the
/// platform would make installation depend on a program being present.
///
/// Failures are reported with the path that failed rather than as a bare false:
/// "could not install" tells the user nothing they can act on.
core::Status copyTree(const QString& from, const QString& to)
{
    QDir().mkpath(to);

    QDirIterator iterator(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        const QString sourcePath = iterator.next();
        const QString relative = QDir(from).relativeFilePath(sourcePath);
        const QString targetPath = QDir(to).filePath(relative);

        const QFileInfo info(sourcePath);
        if (info.isDir()) {
            if (!QDir().mkpath(targetPath)) {
                return core::Err(core::ErrorCode::IoError,
                                 QStringLiteral("Could not create a directory"),
                                 targetPath);
            }
            continue;
        }

        if (!QDir().mkpath(QFileInfo(targetPath).path())) {
            return core::Err(core::ErrorCode::IoError,
                             QStringLiteral("Could not create a directory"),
                             QFileInfo(targetPath).path());
        }
        if (!QFile::copy(sourcePath, targetPath)) {
            return core::Err(core::ErrorCode::IoError,
                             QStringLiteral("Could not copy a file"), relative);
        }
    }
    return core::Ok();
}

} // namespace

core::Result<QString> ExtensionRegistry::installFromDirectory(
    const QString& sourceDirectory)
{
    // Everything is checked before anything is written. A half-installed
    // extension would appear in the panel as broken, and the user would have to
    // work out that they were looking at wreckage rather than a bad extension.
    const QFileInfo source(sourceDirectory);
    if (!source.isDir()) {
        return core::Err(core::ErrorCode::NotFound,
                         QStringLiteral("That is not a folder"), sourceDirectory);
    }

    const core::Result<Manifest> manifest = Manifest::load(sourceDirectory);
    if (!manifest) {
        return core::Err(manifest.error().code(),
                         QStringLiteral("This folder is not a Keys extension: %1")
                             .arg(manifest.error().message()),
                         sourceDirectory);
    }

    const QString entry = QDir(sourceDirectory).filePath(manifest.value().entryPoint);
    if (!QFileInfo::exists(entry)) {
        return core::Err(core::ErrorCode::NotFound,
                         QStringLiteral("The manifest names an entry point that is "
                                        "not in the folder"),
                         manifest.value().entryPoint);
    }

    const QString target =
        QDir(extensionsDirectory()).filePath(manifest.value().id);

    // Refuse to install a folder onto itself, which would delete it.
    if (QFileInfo(target).canonicalFilePath()
        == QFileInfo(sourceDirectory).canonicalFilePath()) {
        return core::Err(core::ErrorCode::InvalidArgument,
                         QStringLiteral("That extension is already installed here"),
                         manifest.value().id);
    }

    // An existing id is an upgrade. The old directory goes entirely rather than
    // being merged: a file the new version dropped would otherwise survive and
    // be loaded.
    if (QFileInfo::exists(target)) {
        if (const core::Status stopped = uninstall(manifest.value().id); !stopped) {
            return core::Err(stopped.error().code(), stopped.error().message(),
                             stopped.error().context());
        }
    }

    if (const core::Status copied = copyTree(sourceDirectory, target); !copied) {
        // Nothing usable is left behind: a directory that failed halfway
        // through copying is not an extension.
        QDir(target).removeRecursively();
        return core::Err(copied.error().code(), copied.error().message(),
                         copied.error().context());
    }

    discover();

    qCInfo(lcCore) << "installed extension" << manifest.value().id
                   << manifest.value().version;

    return manifest.value().id;
}

core::Status ExtensionRegistry::uninstall(const QString& extensionId)
{
    const InstalledExtension* extension = find(extensionId);
    if (!extension) {
        return core::Err(core::ErrorCode::NotFound,
                         QStringLiteral("No such extension is installed"),
                         extensionId);
    }

    const QString directory = extension->directory;

    // Stopped before its files go. Deleting the directory of a running host
    // leaves a process alive with nothing to read, which fails in ways that are
    // hard to attribute.
    m_hosts.erase(extensionId);

    // The grant goes with it. Reinstalling later must ask again rather than
    // inheriting a yes given to a version that no longer exists.
    setGranted(extensionId, {});

    if (!QDir(directory).removeRecursively()) {
        return core::Err(core::ErrorCode::IoError,
                         QStringLiteral("The extension was stopped but its files "
                                        "could not be removed"),
                         directory);
    }

    discover();

    qCInfo(lcCore) << "uninstalled extension" << extensionId;
    return core::Ok();
}

const InstalledExtension* ExtensionRegistry::find(const QString& extensionId) const
{
    const auto it = std::find_if(m_installed.cbegin(), m_installed.cend(),
                                 [&extensionId](const InstalledExtension& extension) {
                                     return extension.manifest.id == extensionId;
                                 });
    return it == m_installed.cend() ? nullptr : &*it;
}

void ExtensionRegistry::ensureGrantKey(const QString& extensionId) const
{
    // Declared on demand: the schema cannot know extension ids in advance, and
    // reading an undeclared key is a warning on every lookup. Declaring it up
    // front also keeps the value validated rather than bypassing the schema.
    if (m_settings.schema().contains(grantKey(extensionId))) {
        return;
    }

    config::SettingDefinition definition;
    definition.key = grantKey(extensionId);
    definition.defaultValue = QString();
    definition.scope = config::SettingScope::Application;
    definition.description =
        QStringLiteral("Capabilities granted to the '%1' extension").arg(extensionId);
    definition.userVisible = false;   // managed from the extensions view
    m_settings.schema().define(std::move(definition));
}

QSet<QString> ExtensionRegistry::grantedCapabilities(const QString& extensionId) const
{
    ensureGrantKey(extensionId);

    const QString stored = m_settings.stringValue(grantKey(extensionId));
    if (stored.isEmpty()) {
        return {};
    }

    const QStringList parts =
        stored.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return QSet<QString>(parts.cbegin(), parts.cend());
}

bool ExtensionRegistry::isFullyGranted(const QString& extensionId) const
{
    const InstalledExtension* extension = find(extensionId);
    if (!extension || !extension->isLoadable()) {
        return false;
    }

    const QSet<QString> granted = grantedCapabilities(extensionId);
    return std::all_of(extension->manifest.capabilities.cbegin(),
                       extension->manifest.capabilities.cend(),
                       [&granted](Capability capability) {
                           return granted.contains(capabilityId(capability));
                       });
}

void ExtensionRegistry::setGranted(const QString& extensionId,
                                   const QSet<QString>& capabilities)
{
    QStringList sorted(capabilities.cbegin(), capabilities.cend());
    sorted.sort();   // stable on disk, so a diff of the settings file is readable

    ensureGrantKey(extensionId);
    m_settings.setValue(grantKey(extensionId), sorted.join(QLatin1Char(',')));

    // Revoking has to actually take effect. Restarting is the only honest way:
    // the extension was told its capabilities at startup and may hold handles
    // it obtained under the old grant.
    const bool running = isRunning(extensionId);
    if (running) {
        stopExtension(extensionId);
    }

    if (!m_workspaceRoot.isEmpty() && isFullyGranted(extensionId)) {
        if (const InstalledExtension* extension = find(extensionId)) {
            startExtension(*extension, m_workspaceRoot);
        }
    }

    emit runningChanged();
}

bool ExtensionRegistry::isRunning(const QString& extensionId) const
{
    const auto it = m_hosts.find(extensionId);
    return it != m_hosts.cend() && it->second && it->second->isRunning();
}

void ExtensionRegistry::activateAll(const QString& workspaceRoot)
{
    m_workspaceRoot = workspaceRoot;

    for (const InstalledExtension& extension : m_installed) {
        if (!extension.isLoadable()) {
            continue;
        }
        // Only when everything it asked for was granted. Starting an extension
        // with a partial grant means it fails at some unpredictable later point
        // rather than plainly not starting.
        if (isFullyGranted(extension.manifest.id)) {
            startExtension(extension, workspaceRoot);
        }
    }
    emit runningChanged();
}

void ExtensionRegistry::deactivateAll()
{
    const QStringList ids = [this] {
        QStringList result;
        for (const auto& [id, host] : m_hosts) {
            result << id;
        }
        return result;
    }();

    for (const QString& id : ids) {
        stopExtension(id);
    }

    m_workspaceRoot.clear();
    emit runningChanged();
}

void ExtensionRegistry::startExtension(const InstalledExtension& extension,
                                       const QString& workspaceRoot)
{
    if (isRunning(extension.manifest.id)) {
        return;
    }

    auto host = std::make_unique<ExtensionHost>(extension.manifest, extension.directory);
    ExtensionHost* raw = host.get();
    const QString name = extension.manifest.name;

    connect(raw, &ExtensionHost::showMessageRequested, this, &ExtensionRegistry::notice);

    connect(raw, &ExtensionHost::failed, this, &ExtensionRegistry::notice);

    connect(raw, &ExtensionHost::capabilityDenied, this,
            [this, name](const QString& method, const QString& capability) {
                // Surfaced so a user can see an extension reaching beyond its
                // grant, rather than it failing invisibly.
                emit notice(tr("'%1' tried to use %2 (%3), which it was not granted.")
                                .arg(name, capability, method));
            });

    connect(raw, &ExtensionHost::logged, this, [name](const QString& text) {
        qCDebug(lcCore) << name << ":" << text;
    });

    connect(raw, &ExtensionHost::stateChanged, this, &ExtensionRegistry::runningChanged);

    if (const core::Status status =
            host->start(grantedCapabilities(extension.manifest.id), workspaceRoot);
        !status) {
        emit notice(status.error().message());
        return;
    }

    m_hosts.emplace(extension.manifest.id, std::move(host));
    registerContributions(extension);
}

void ExtensionRegistry::stopExtension(const QString& extensionId)
{
    unregisterContributions(extensionId);

    const auto it = m_hosts.find(extensionId);
    if (it == m_hosts.end()) {
        return;
    }

    if (it->second) {
        it->second->stop();
    }
    m_hosts.erase(it);
}

void ExtensionRegistry::registerContributions(const InstalledExtension& extension)
{
    QStringList registered;

    for (const CommandContribution& contribution : extension.manifest.commands) {
        const QString extensionId = extension.manifest.id;
        const QString commandId = contribution.id;

        core::Command command;
        command.id = commandId;
        command.title = contribution.title;
        command.category = contribution.category.isEmpty() ? extension.manifest.name
                                                           : contribution.category;

        command.handler = [this, extensionId, commandId] {
            const auto it = m_hosts.find(extensionId);
            if (it != m_hosts.end() && it->second) {
                it->second->invokeCommand(commandId);
            }
        };

        // Enabled only while its extension is running, so a command cannot be
        // invoked into a process that is not there.
        command.isEnabled = [this, extensionId] { return isRunning(extensionId); };

        if (const core::Status status = m_commands.registerCommand(std::move(command));
            status) {
            registered << commandId;
        } else {
            qCWarning(lcCore) << "could not register" << commandId << ":"
                              << status.error().toString();
        }
    }

    if (!registered.isEmpty()) {
        m_registeredCommands.insert(extension.manifest.id, registered);
    }
}

void ExtensionRegistry::unregisterContributions(const QString& extensionId)
{
    // Exactly what was registered, so stopping one extension cannot withdraw
    // another's commands.
    for (const QString& commandId : m_registeredCommands.take(extensionId)) {
        m_commands.unregisterCommand(commandId);
    }
}

} // namespace keys::extensions
