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

    /// Heading the settings UI files this under ("Appearance", "Editor").
    /// Derived from the key's first segment when empty.
    QString group;

    /// Short label for the settings UI. The key is not a label: "editor.fontSize"
    /// is an identifier, "Font size" is what a person reads.
    QString title;

    /// Inclusive bounds for numeric settings. Equal values mean unbounded.
    /// These are what stop a slider offering an 800-point font, and they are
    /// enforced on write rather than only in the UI - a hand-edited settings
    /// file must not be able to produce an unusable editor.
    double minimum = 0.0;
    double maximum = 0.0;

    /// False for state the application persists but the user does not set
    /// directly - sidebar width, which view was open. Keeping these in the
    /// schema means they are still validated and still round-trip through the
    /// same file; hiding them keeps the settings UI to actual preferences.
    bool userVisible = true;

    [[nodiscard]] bool isBounded() const { return minimum < maximum; }
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
