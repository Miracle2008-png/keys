#pragma once

#include "config/SettingsSchema.h"
#include "core/Result.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariant>

namespace keys::config {

/// Layered application settings.
///
/// Three layers resolved in order: defaults (from the schema) → user → workspace.
/// A lookup returns the value from the highest layer that has one, so a workspace
/// can override the user, who overrides the built-in default, and clearing a layer
/// reveals what is underneath rather than falling to zero.
///
/// Values are read through typed accessors and validated against the schema on
/// write, so an unknown key or a wrong type fails loudly instead of becoming an
/// invisible no-op.
class Settings : public QObject {
    Q_OBJECT

public:
    enum class Layer {
        Default,    ///< from the schema; read-only
        User,       ///< the user's global preferences
        Workspace,  ///< overrides for the open project
    };

    explicit Settings(QObject* parent = nullptr);

    [[nodiscard]] SettingsSchema& schema() { return m_schema; }
    [[nodiscard]] const SettingsSchema& schema() const { return m_schema; }

    /// Resolved value: workspace, else user, else the schema default.
    /// Returns an invalid QVariant for an undeclared key.
    [[nodiscard]] QVariant value(const QString& key) const;

    [[nodiscard]] QString stringValue(const QString& key) const;
    [[nodiscard]] int intValue(const QString& key) const;
    [[nodiscard]] double doubleValue(const QString& key) const;
    [[nodiscard]] bool boolValue(const QString& key) const;

    /// Writes to a layer. Fails if the key is undeclared, the value is invalid for
    /// it, the target is Default (read-only), or an Application-scoped setting is
    /// written to the Workspace layer.
    core::Status setValue(const QString& key, const QVariant& value, Layer layer = Layer::User);

    /// Removes a key from a layer, revealing the layer beneath it.
    core::Status clearValue(const QString& key, Layer layer);

    /// True if `layer` holds an explicit value for `key`.
    [[nodiscard]] bool isSetIn(const QString& key, Layer layer) const;

    /// Drops every workspace override. Called when a project closes so one
    /// project's settings cannot leak into the next.
    void clearWorkspaceLayer();

    /// Replaces a layer wholesale, emitting a change for each key that differs.
    /// Used when loading settings from disk.
    void replaceLayer(Layer layer, const QHash<QString, QVariant>& values);

    [[nodiscard]] QHash<QString, QVariant> layerValues(Layer layer) const;

signals:
    /// Emitted once per key whose resolved value actually changed. Nothing polls
    /// settings; consumers connect to this.
    void changed(const QString& key, const QVariant& newValue);

private:
    [[nodiscard]] const QHash<QString, QVariant>& layerMap(Layer layer) const;
    [[nodiscard]] QHash<QString, QVariant>& layerMap(Layer layer);

    SettingsSchema m_schema;
    QHash<QString, QVariant> m_user;
    QHash<QString, QVariant> m_workspace;
};

} // namespace keys::config
