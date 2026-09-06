#include "config/SettingsSchema.h"

namespace keys::config {

SettingsSchema::SettingsSchema()
{
    defineBuiltins();
}

void SettingsSchema::define(SettingDefinition definition)
{
    if (definition.key.isEmpty()) {
        return;
    }
    if (!m_definitions.contains(definition.key)) {
        m_order.append(definition.key);
    }
    m_definitions.insert(definition.key, std::move(definition));
}

bool SettingsSchema::contains(const QString& key) const
{
    return m_definitions.contains(key);
}

const SettingDefinition* SettingsSchema::find(const QString& key) const
{
    const auto it = m_definitions.constFind(key);
    return it == m_definitions.cend() ? nullptr : &it.value();
}

QList<SettingDefinition> SettingsSchema::all() const
{
    QList<SettingDefinition> result;
    result.reserve(m_order.size());
    for (const QString& key : m_order) {
        const auto it = m_definitions.constFind(key);
        if (it != m_definitions.cend()) {
            result.append(it.value());
        }
    }
    return result;
}

bool SettingsSchema::isValid(const QString& key, const QVariant& value) const
{
    const SettingDefinition* definition = find(key);
    if (!definition) {
        return false;
    }

    // The default fixes the type. A value that cannot convert to it is a bug in
    // the caller or a corrupted settings file; either way it must not be stored.
    if (!value.canConvert(definition->defaultValue.metaType())) {
        return false;
    }

    if (!definition->allowedValues.isEmpty()) {
        return definition->allowedValues.contains(value.toString());
    }

    // Bounds are enforced here rather than only in the UI. Settings arrive from
    // a hand-edited JSON file as readily as from a slider, and a font size of
    // 800 would leave the editor unusable with no obvious way back.
    if (definition->isBounded()) {
        bool numeric = false;
        const double number = value.toDouble(&numeric);
        if (!numeric) {
            return false;
        }
        return number >= definition->minimum && number <= definition->maximum;
    }
    return true;
}

void SettingsSchema::defineBuiltins()
{
    // Only settings that something actually honours are declared. A key with no
    // consumer would surface in the settings UI as a control that changes
    // nothing, which is worse than the setting not existing: the user cannot
    // tell a preference that does not apply from one that is broken. Minimap,
    // format-on-save and the terminal's settings return with the features that
    // read them.

    // ---- Appearance -------------------------------------------------------
    define({.key = QStringLiteral("appearance.theme"),
            .defaultValue = QStringLiteral("dark"),
            .scope = SettingScope::Application,
            .description = QStringLiteral("Interface color mode"),
            .allowedValues = {QStringLiteral("dark"), QStringLiteral("light")},
            .group = QStringLiteral("Appearance"),
            .title = QStringLiteral("Theme")});

    // Drives the design's --anim-t token application-wide. See AnimationPolicy.
    define({.key = QStringLiteral("animation.level"),
            .defaultValue = QStringLiteral("full"),
            .scope = SettingScope::Application,
            .description = QStringLiteral("Controls motion across the whole interface"),
            .allowedValues = {QStringLiteral("full"), QStringLiteral("reduced"),
                              QStringLiteral("off")},
            .group = QStringLiteral("Appearance"),
            .title = QStringLiteral("Animation")});

    define({.key = QStringLiteral("accessibility.uiScale"),
            .defaultValue = 1.0,
            .scope = SettingScope::Application,
            .description = QStringLiteral("Scale factor applied to interface text and controls"),
            .group = QStringLiteral("Appearance"),
            .title = QStringLiteral("Interface scale"),
            .minimum = 0.8,
            .maximum = 2.0});

    // ---- Editor -----------------------------------------------------------
    define({.key = QStringLiteral("editor.fontSize"),
            .defaultValue = 13.0,
            .scope = SettingScope::Workspace,
            .description = QStringLiteral("Editor font size in pixels"),
            .group = QStringLiteral("Editor"),
            .title = QStringLiteral("Font size"),
            .minimum = 8.0,
            .maximum = 32.0});

    define({.key = QStringLiteral("editor.fontFamily"),
            .defaultValue = QString(),
            .scope = SettingScope::Workspace,
            .description = QStringLiteral("Editor font family; empty uses the platform default"),
            .group = QStringLiteral("Editor"),
            .title = QStringLiteral("Font family")});

    define({.key = QStringLiteral("editor.lineHeight"),
            .defaultValue = 1.6,
            .scope = SettingScope::Workspace,
            .description = QStringLiteral("Line height as a multiple of font size"),
            .group = QStringLiteral("Editor"),
            .title = QStringLiteral("Line height"),
            .minimum = 1.0,
            .maximum = 3.0});

    define({.key = QStringLiteral("editor.tabSize"),
            .defaultValue = 4,
            .scope = SettingScope::Workspace,
            .description = QStringLiteral("Number of spaces a tab represents"),
            .group = QStringLiteral("Editor"),
            .title = QStringLiteral("Tab size"),
            .minimum = 1.0,
            .maximum = 16.0});

    define({.key = QStringLiteral("editor.insertSpaces"),
            .defaultValue = true,
            .scope = SettingScope::Workspace,
            .description = QStringLiteral("Insert spaces when pressing Tab"),
            .group = QStringLiteral("Editor"),
            .title = QStringLiteral("Insert spaces")});

    // ---- Workbench --------------------------------------------------------
    // Window state rather than preference: the application persists these so a
    // session resumes as it was left, but the user sets them by dragging the
    // sidebar, not by opening settings.
    define({.key = QStringLiteral("workbench.sidebarWidth"),
            .defaultValue = 268,
            .scope = SettingScope::Application,
            .description = QStringLiteral("Width of the sidebar in pixels"),
            .group = QStringLiteral("Workbench"),
            .title = QStringLiteral("Sidebar width"),
            .minimum = 180.0,
            .maximum = 640.0,
            .userVisible = false});

    define({.key = QStringLiteral("workbench.sidebarVisible"),
            .defaultValue = true,
            .scope = SettingScope::Application,
            .description = QStringLiteral("Whether the sidebar is shown"),
            .group = QStringLiteral("Workbench"),
            .title = QStringLiteral("Show sidebar"),
            .userVisible = false});

    define({.key = QStringLiteral("workbench.activeView"),
            .defaultValue = QStringLiteral("explorer"),
            .scope = SettingScope::Application,
            .description = QStringLiteral("Which sidebar view is active"),
            .allowedValues = {QStringLiteral("explorer"), QStringLiteral("search"),
                              QStringLiteral("sourceControl"), QStringLiteral("debug"),
                              QStringLiteral("extensions")},
            .group = QStringLiteral("Workbench"),
            .title = QStringLiteral("Active view"),
            .userVisible = false});

    // ---- Updates ----------------------------------------------------------
    // Declared here rather than written straight to the file so the opt-out is
    // a real setting: undeclared keys are refused by setValue, which meant the
    // switch appeared to work and persisted nothing.
    define({.key = QStringLiteral("updates.checkAutomatically"),
            .defaultValue = true,
            .scope = SettingScope::Application,
            .description = QStringLiteral(
                "Check once a day whether a newer release of Keys exists. "
                "This is the only outbound request Keys makes; turning it off "
                "means no request is sent."),
            .group = QStringLiteral("Updates"),
            .title = QStringLiteral("Check for updates automatically")});

    // When the last check ran, so a launch does not re-check what was checked
    // an hour ago. State, not preference - hence not user-visible.
    define({.key = QStringLiteral("updates.lastCheck"),
            .defaultValue = QString(),
            .scope = SettingScope::Application,
            .description = QStringLiteral("When the last update check ran"),
            .group = QStringLiteral("Updates"),
            .title = QStringLiteral("Last update check"),
            .userVisible = false});
}

} // namespace keys::config
