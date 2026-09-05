#include "ui/ExtensionsModel.h"

#include <QVariantMap>

using keys::extensions::Capability;
using keys::extensions::InstalledExtension;

namespace keys::ui {
namespace {

/// The instance main() publishes for QML; see Theme.cpp for the reasoning.
ExtensionsModel* g_instance = nullptr;

} // namespace

void ExtensionsModel::setInstance(ExtensionsModel* instance)
{
    g_instance = instance;
}

ExtensionsModel* ExtensionsModel::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Q_ASSERT_X(g_instance, "ExtensionsModel::create",
               "ExtensionsModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

ExtensionsModel::ExtensionsModel(extensions::ExtensionRegistry& registry, QObject* parent)
    : QAbstractListModel(parent), m_registry(registry)
{
    connect(&m_registry, &extensions::ExtensionRegistry::extensionsChanged,
            this, &ExtensionsModel::rebuild);

    // Running state changes the row without changing the list, so this is a
    // data change rather than a reset - a reset would collapse any expanded
    // capability list the user was reading.
    connect(&m_registry, &extensions::ExtensionRegistry::runningChanged, this, [this] {
        if (count() > 0) {
            emit dataChanged(index(0, 0), index(count() - 1, 0),
                             {RunningRole, GrantedRole, CapabilitiesRole});
        }
        emit changed();
    });
}

QString ExtensionsModel::directory()
{
    return extensions::ExtensionRegistry::extensionsDirectory();
}

int ExtensionsModel::count() const
{
    return static_cast<int>(m_registry.installed().size());
}

void ExtensionsModel::rebuild()
{
    beginResetModel();
    endResetModel();
    emit changed();
}

void ExtensionsModel::refresh()
{
    m_registry.discover();
}

int ExtensionsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant ExtensionsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const InstalledExtension& extension =
        m_registry.installed().at(static_cast<size_t>(index.row()));

    switch (role) {
    case IdRole:
        return extension.manifest.id;
    case NameRole:
        return extension.manifest.name;
    case VersionRole:
        return extension.manifest.version;
    case DescriptionRole:
        return extension.manifest.description;
    case AuthorRole:
        return extension.manifest.author;
    case RunningRole:
        return m_registry.isRunning(extension.manifest.id);
    case GrantedRole:
        return m_registry.isFullyGranted(extension.manifest.id);
    case ErrorRole:
        return extension.error;

    case CapabilitiesRole: {
        // Plain language, not API names: a user cannot consent to
        // "modifyEditor", but can to "Change the text in your editor".
        const QSet<QString> granted =
            m_registry.grantedCapabilities(extension.manifest.id);

        QVariantList result;
        for (const Capability capability : extension.manifest.capabilities) {
            QVariantMap entry;
            entry.insert(QStringLiteral("id"), extensions::capabilityId(capability));
            entry.insert(QStringLiteral("description"),
                         extensions::capabilityDescription(capability));
            entry.insert(QStringLiteral("sensitive"),
                         extensions::capabilityIsSensitive(capability));
            entry.insert(QStringLiteral("granted"),
                         granted.contains(extensions::capabilityId(capability)));
            result.append(entry);
        }
        return result;
    }

    default:
        return {};
    }
}

QHash<int, QByteArray> ExtensionsModel::roleNames() const
{
    return {
        {IdRole, "extensionId"},
        {NameRole, "name"},
        {VersionRole, "version"},
        {DescriptionRole, "description"},
        {AuthorRole, "author"},
        {RunningRole, "running"},
        {GrantedRole, "granted"},
        {ErrorRole, "error"},
        {CapabilitiesRole, "capabilities"},
    };
}

void ExtensionsModel::grantAll(const QString& extensionId)
{
    const InstalledExtension* found = nullptr;
    for (const InstalledExtension& extension : m_registry.installed()) {
        if (extension.manifest.id == extensionId) {
            found = &extension;
            break;
        }
    }
    if (!found || !found->isLoadable()) {
        return;
    }

    QSet<QString> capabilities;
    for (const Capability capability : found->manifest.capabilities) {
        capabilities.insert(extensions::capabilityId(capability));
    }
    m_registry.setGranted(extensionId, capabilities);
}

void ExtensionsModel::revokeAll(const QString& extensionId)
{
    // An empty grant, which stops the extension: a revoked capability that left
    // it running would be no revocation at all.
    m_registry.setGranted(extensionId, {});
}

} // namespace keys::ui
