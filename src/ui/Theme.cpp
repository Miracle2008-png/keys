#include "ui/Theme.h"

#include "ui/OklchColor.h"

namespace keys::ui {
namespace {

constexpr auto kThemeKey = "appearance.theme";

/// The design's palette, transcribed from the handoff in its original OKLCH form
/// so it can be verified against the design document directly.
struct Palette {
    QColor bgChrome, bgSurface, bgEditor, bgElevated, bgHover;
    QColor border, borderStrong;
    QColor textPrimary, textSecondary, textTertiary;
    QColor accent, accentHover, accentSoft, accentSoftBorder;
    QColor green, greenSoft, red, redSoft, yellow;
    QColor synKeyword, synString, synNumber, synFunction, synType;
    QColor synTag, synComment, synPlain, synPunct;
};

const Palette& darkPalette()
{
    // Built once on first use; the conversion is not free and the values never change.
    static const Palette palette = [] {
        Palette p;
        p.bgChrome         = oklch(0.165, 0.005, 255);
        p.bgSurface        = oklch(0.19,  0.005, 255);
        p.bgEditor         = oklch(0.2,   0.005, 255);
        p.bgElevated       = oklch(0.235, 0.006, 255);
        p.bgHover          = oklch(0.255, 0.006, 255);

        // Borders are white at low alpha so they read consistently over any surface.
        p.border           = oklch(1.0, 0.0, 0.0, 0.08);
        p.borderStrong     = oklch(1.0, 0.0, 0.0, 0.16);

        p.textPrimary      = oklch(0.94, 0.004, 255);
        p.textSecondary    = oklch(0.63, 0.01,  255);
        p.textTertiary     = oklch(0.45, 0.01,  255);

        p.accent           = oklch(0.64, 0.1, 252);
        p.accentHover      = oklch(0.7,  0.1, 252);
        p.accentSoft       = oklch(0.64, 0.1, 252, 0.16);
        p.accentSoftBorder = oklch(0.64, 0.1, 252, 0.4);

        p.green            = oklch(0.68, 0.11, 152);
        p.greenSoft        = oklch(0.68, 0.11, 152, 0.15);
        p.red              = oklch(0.66, 0.15, 25);
        p.redSoft          = oklch(0.66, 0.15, 25, 0.14);
        p.yellow           = oklch(0.78, 0.12, 95);

        p.synKeyword       = oklch(0.68, 0.12, 300);
        p.synString        = oklch(0.72, 0.1,  150);
        p.synNumber        = oklch(0.75, 0.11, 60);
        p.synFunction      = oklch(0.72, 0.1,  252);
        p.synType          = oklch(0.75, 0.08, 190);
        p.synTag           = oklch(0.68, 0.12, 25);
        p.synComment       = oklch(0.45, 0.01, 255);
        p.synPlain         = oklch(0.94, 0.004, 255);
        p.synPunct         = oklch(0.63, 0.01, 255);
        return p;
    }();
    return palette;
}

const Palette& lightPalette()
{
    static const Palette palette = [] {
        Palette p;
        p.bgChrome         = oklch(0.965, 0.003, 255);
        p.bgSurface        = oklch(0.975, 0.003, 255);
        p.bgEditor         = oklch(0.99,  0.002, 255);
        p.bgElevated       = oklch(1.0,   0.0,   0.0);
        p.bgHover          = oklch(0.93,  0.004, 255);

        // Black at low alpha in light mode, mirroring the dark theme's approach.
        p.border           = oklch(0.0, 0.0, 0.0, 0.09);
        p.borderStrong     = oklch(0.0, 0.0, 0.0, 0.18);

        p.textPrimary      = oklch(0.24, 0.006, 255);
        p.textSecondary    = oklch(0.48, 0.01,  255);
        p.textTertiary     = oklch(0.62, 0.01,  255);

        p.accent           = oklch(0.52, 0.11, 252);
        p.accentHover      = oklch(0.46, 0.11, 252);
        p.accentSoft       = oklch(0.52, 0.11, 252, 0.1);
        p.accentSoftBorder = oklch(0.52, 0.11, 252, 0.32);

        p.green            = oklch(0.5,  0.12, 152);
        p.greenSoft        = oklch(0.5,  0.12, 152, 0.12);
        p.red              = oklch(0.55, 0.17, 25);
        p.redSoft          = oklch(0.55, 0.17, 25, 0.1);
        p.yellow           = oklch(0.62, 0.14, 95);

        p.synKeyword       = oklch(0.48, 0.14, 300);
        p.synString        = oklch(0.45, 0.12, 150);
        p.synNumber        = oklch(0.5,  0.13, 60);
        p.synFunction      = oklch(0.48, 0.12, 252);
        p.synType          = oklch(0.48, 0.1,  190);
        p.synTag           = oklch(0.5,  0.14, 25);
        p.synComment       = oklch(0.62, 0.01, 255);
        p.synPlain         = oklch(0.24, 0.006, 255);
        p.synPunct         = oklch(0.48, 0.01, 255);
        return p;
    }();
    return palette;
}

/// The instance main() publishes for QML. A raw pointer, not an owning one: the
/// application owns the Theme, and QML is only given a view of it.
Theme* g_instance = nullptr;

} // namespace

Theme::Theme(config::Settings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
    applyFromSettings(m_settings.value(QLatin1String(kThemeKey)));

    // Settings are the source of truth: a change from anywhere - the settings
    // page, a toggle, a reload from disk - reaches the theme through this one
    // connection.
    connect(&m_settings, &config::Settings::changed, this,
            [this](const QString& key, const QVariant& value) {
                if (key == QLatin1String(kThemeKey)) {
                    applyFromSettings(value);
                }
            });
}

void Theme::applyFromSettings(const QVariant& value)
{
    const Mode resolved =
        value.toString() == QLatin1String("light") ? Mode::Light : Mode::Dark;
    if (resolved != m_mode) {
        m_mode = resolved;
        emit changed();
    }
}

void Theme::setInstance(Theme* instance)
{
    g_instance = instance;
}

Theme* Theme::instance()
{
    return g_instance;
}

Theme* Theme::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    // A null instance means main() did not publish one before loading QML, which
    // is a wiring bug rather than a runtime condition to recover from.
    Q_ASSERT_X(g_instance, "Theme::create", "Theme::setInstance was not called");

    // The engine must not take ownership of an object the application owns.
    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}


void Theme::setMode(Mode mode)
{
    if (mode == m_mode) {
        return;
    }

    // Written through settings rather than assigning m_mode directly, so the
    // change is persisted and every other observer is notified. The settings
    // callback applies it back to us.
    m_settings.setValue(QLatin1String(kThemeKey),
                        mode == Mode::Light ? QStringLiteral("light")
                                            : QStringLiteral("dark"));
}

void Theme::toggleMode()
{
    setMode(m_mode == Mode::Dark ? Mode::Light : Mode::Dark);
}

// Every accessor resolves through the active palette, so a mode change moves the
// whole interface at once via the single `changed` signal.
#define KEYS_THEME_COLOR(name)                                                  \
    QColor Theme::name() const                                                  \
    {                                                                           \
        return (m_mode == Mode::Dark ? darkPalette() : lightPalette()).name;    \
    }

KEYS_THEME_COLOR(bgChrome)
KEYS_THEME_COLOR(bgSurface)
KEYS_THEME_COLOR(bgEditor)
KEYS_THEME_COLOR(bgElevated)
KEYS_THEME_COLOR(bgHover)
KEYS_THEME_COLOR(border)
KEYS_THEME_COLOR(borderStrong)
KEYS_THEME_COLOR(textPrimary)
KEYS_THEME_COLOR(textSecondary)
KEYS_THEME_COLOR(textTertiary)
KEYS_THEME_COLOR(accent)
KEYS_THEME_COLOR(accentHover)
KEYS_THEME_COLOR(accentSoft)
KEYS_THEME_COLOR(accentSoftBorder)
KEYS_THEME_COLOR(green)
KEYS_THEME_COLOR(greenSoft)
KEYS_THEME_COLOR(red)
KEYS_THEME_COLOR(redSoft)
KEYS_THEME_COLOR(yellow)
KEYS_THEME_COLOR(synKeyword)
KEYS_THEME_COLOR(synString)
KEYS_THEME_COLOR(synNumber)
KEYS_THEME_COLOR(synFunction)
KEYS_THEME_COLOR(synType)
KEYS_THEME_COLOR(synTag)
KEYS_THEME_COLOR(synComment)
KEYS_THEME_COLOR(synPlain)
KEYS_THEME_COLOR(synPunct)

#undef KEYS_THEME_COLOR

} // namespace keys::ui
