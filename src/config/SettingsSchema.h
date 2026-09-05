#pragma once

#include <QHash>
#include <QString>
#include <QVariant>

namespace keys::config {

/// Which layer a setting may be written to.
enum class SettingScope {
    /// Applies to the whole application; a workspace cannot override it.
    /// Window and UI preferences that would be disorienting if they changed
    /// per project.
    Application,

    /// May be overridden per workspace. Anything a project legitimately has an
    /// opinion about — tab width, formatter, build command.
    Workspace,
};

/// Declaration of one setting: its key, type, default and where it may be set.
///
/// Every setting is declared exactly once. The schema is what lets the settings UI
/// be generated rather than hand-maintained, and what makes an unknown or
/// wrong-typed key a detectable error instead of a silent default.
struct SettingDefinition {
    QString key;              ///< dotted: "editor.fontSize", "animation.level"
    QVariant defaultValue;    ///< also fixes the setting's type
    SettingScope scope = SettingScope::Workspace;
    QString description;      ///< shown in the settings UI

    /// Allowed values for enumerated settings. Empty means unconstrained.
    QStringList allowedValues;
};

/// The registry of known settings.
///
/// Keys does not accept arbitrary keys: writing an undeclared key fails. That
/// prevents typos from becoming invisible no-ops, and means the settings UI can
/// always show every setting that exists.
class SettingsSchema {
public:
    SettingsSchema();

    void define(SettingDefinition definition);

    [[nodiscard]] bool contains(const QString& key) const;
    [[nodiscard]] const SettingDefinition* find(const QString& key) const;
    [[nodiscard]] QList<SettingDefinition> all() const;

    /// True if `value` is acceptable for `key`: the key exists, the type converts,
    /// and the value is within allowedValues when constrained.
    [[nodiscard]] bool isValid(const QString& key, const QVariant& value) const;

private:
    /// Registers the settings Keys itself owns. Modules add their own via define().
    void defineBuiltins();

    QHash<QString, SettingDefinition> m_definitions;
    QStringList m_order;
};

} // namespace keys::config
