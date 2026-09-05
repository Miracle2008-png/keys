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
    return true;
}

void SettingsSchema::defineBuiltins()
{
    // ---- Appearance -------------------------------------------------------
    define({QStringLiteral("appearance.theme"),
            QStringLiteral("dark"),
            SettingScope::Application,
            QStringLiteral("Interface color mode"),
            {QStringLiteral("dark"), QStringLiteral("light")}});

    // Drives the design's --anim-t token application-wide. See AnimationPolicy.
    define({QStringLiteral("animation.level"),
            QStringLiteral("full"),
            SettingScope::Application,
            QStringLiteral("Controls motion across the whole interface"),
            {QStringLiteral("full"), QStringLiteral("reduced"), QStringLiteral("off")}});

    // ---- Editor -----------------------------------------------------------
    define({QStringLiteral("editor.fontSize"),
            13,
            SettingScope::Workspace,
            QStringLiteral("Editor font size in pixels")});

    define({QStringLiteral("editor.fontFamily"),
            QStringLiteral("JetBrains Mono"),
            SettingScope::Workspace,
            QStringLiteral("Editor font family")});

    define({QStringLiteral("editor.lineHeight"),
            1.6,
            SettingScope::Workspace,
            QStringLiteral("Line height as a multiple of font size")});

    define({QStringLiteral("editor.tabSize"),
            4,
            SettingScope::Workspace,
            QStringLiteral("Number of spaces a tab represents")});

    define({QStringLiteral("editor.insertSpaces"),
            true,
            SettingScope::Workspace,
            QStringLiteral("Insert spaces when pressing Tab")});

    define({QStringLiteral("editor.minimap"),
            true,
            SettingScope::Workspace,
            QStringLiteral("Show a scaled preview on the right edge")});

    define({QStringLiteral("editor.formatOnSave"),
            false,
            SettingScope::Workspace,
            QStringLiteral("Format the document when it is saved")});

    // ---- Workbench --------------------------------------------------------
    define({QStringLiteral("workbench.sidebarWidth"),
            268,
            SettingScope::Application,
            QStringLiteral("Width of the sidebar in pixels")});

    define({QStringLiteral("workbench.sidebarVisible"),
            true,
            SettingScope::Application,
            QStringLiteral("Whether the sidebar is shown")});

    define({QStringLiteral("workbench.activeView"),
            QStringLiteral("explorer"),
            SettingScope::Application,
            QStringLiteral("Which sidebar view is active"),
            {QStringLiteral("explorer"), QStringLiteral("search"),
             QStringLiteral("sourceControl"), QStringLiteral("debug"),
             QStringLiteral("extensions")}});

    // ---- Terminal ---------------------------------------------------------
    // Empty means "use the platform default shell", resolved by the terminal
    // module rather than baked into the schema.
    define({QStringLiteral("terminal.shell"),
            QString(),
            SettingScope::Workspace,
            QStringLiteral("Default shell for new terminals")});

    define({QStringLiteral("terminal.fontSize"),
            12.5,
            SettingScope::Workspace,
            QStringLiteral("Terminal font size in pixels")});

    define({QStringLiteral("terminal.height"),
            230,
            SettingScope::Application,
            QStringLiteral("Height of the terminal panel in pixels")});

    // ---- Accessibility ----------------------------------------------------
    define({QStringLiteral("accessibility.uiScale"),
            1.0,
            SettingScope::Application,
            QStringLiteral("Scale factor applied to interface text and controls")});
}

} // namespace keys::config
