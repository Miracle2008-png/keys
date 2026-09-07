#pragma once

#include "extensions/ExtensionRegistry.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QVariantList>

namespace keys::ui {

/// The installed extensions and their grants.
///
/// **The grant prompt is the point of this view.** An extension declares what it
/// needs; the user sees that list in plain language and approves it or does not.
/// Nothing runs before that, which is what makes the capability model real
/// rather than decorative.
class ExtensionsModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(Extensions)
    QML_SINGLETON

    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QString directory READ directory CONSTANT)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        VersionRole,
        DescriptionRole,
        AuthorRole,
        RunningRole,
        GrantedRole,       ///< every declared capability has been approved
        ErrorRole,         ///< why it could not be loaded, if it could not
        CapabilitiesRole,  ///< list of {id, description, sensitive, granted}
    };

    explicit ExtensionsModel(extensions::ExtensionRegistry& registry,
                             QObject* parent = nullptr);

    static void setInstance(ExtensionsModel* instance);
    static ExtensionsModel* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const;

    /// Where extensions are installed, so the view can tell the user where to
    /// put one.
    [[nodiscard]] static QString directory();

    /// Approves everything an extension asked for, and starts it. All or
    /// nothing: a partial grant makes an extension fail at some unpredictable
    /// later point rather than plainly not starting.
    Q_INVOKABLE void grantAll(const QString& extensionId);

    /// Withdraws every grant, which stops the extension.
    Q_INVOKABLE void revokeAll(const QString& extensionId);

    Q_INVOKABLE void refresh();

    /// Installs from a folder containing `keys-extension.json`.
    ///
    /// Reports through `installFailed` rather than returning: the picker that
    /// supplies the folder is asynchronous, so the caller is no longer waiting
    /// by the time this finishes.
    Q_INVOKABLE void installFromFolder(const QString& folder);

    Q_INVOKABLE void uninstall(const QString& extensionId);

signals:
    /// An extension was installed. Carries the name so the notice can say which
    /// one, and how many capabilities it is waiting to be granted.
    void installed(const QString& name, int pendingCapabilities);

    void installFailed(const QString& reason);
    void uninstalled(const QString& name);

    void changed();

private:
    void rebuild();

    extensions::ExtensionRegistry& m_registry;
};

} // namespace keys::ui
