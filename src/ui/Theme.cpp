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
    QColor synPreproc, synConstant, synOperator, synAttribute;
};

const Palette& darkPalette()
{
    // Built once on first use; the conversion is not free and the values never change.
    static const Palette palette = [] {
        Palette p;
        // Surfaces carry a blue-grey cast rather than being neutral, the way
        // One Dark does. A palette at chroma 0.005 is technically grey and
        // reads as dead; the small amount of colour here is what makes a dark
        // interface feel lit rather than switched off.
        //
        // The steps between them are deliberate and large enough to see. The
        // previous values sat within 3.5% lightness of each other, so the
        // sidebar, the editor and the chrome all read as one flat sheet and
        // nothing on screen had an edge. The editor is the lightest of the
        // three because it is where the work happens - the eye should settle
        // there, and the surrounding chrome should recede.
        // Deeper and slightly warmer than before. The editor is the lightest
        // surface because that is where the work is; the chrome around it
        // recedes. Hue 255 rather than 264 takes the violet cast off, which
        // was reading as a theme rather than as a neutral dark interface.
        p.bgChrome         = oklch(0.185, 0.010, 255);
        p.bgSurface        = oklch(0.215, 0.011, 255);
        p.bgEditor         = oklch(0.245, 0.012, 255);
        p.bgElevated       = oklch(0.285, 0.013, 255);
        p.bgHover          = oklch(0.315, 0.014, 255);

        // Borders are white at low alpha so they read consistently over any
        // surface. Stronger than before: panels in CLion have visible edges,
        // and an edge is what turns a region into a panel.
        p.border           = oklch(1.0, 0.0, 0.0, 0.12);
        p.borderStrong     = oklch(1.0, 0.0, 0.0, 0.22);

        // One Dark's foreground is a soft off-white, never pure. Secondary and
        // tertiary keep enough contrast to be read rather than merely seen.
        p.textPrimary      = oklch(0.90, 0.008, 264);
        p.textSecondary    = oklch(0.70, 0.012, 264);
        p.textTertiary     = oklch(0.55, 0.012, 264);

        // Teal rather than blue. Hue 252 is the blue every UI framework ships
        // with, and it made Keys look like a web dashboard; 195 is the hue
        // JetBrains uses for selection and CLion for its own accents. It reads
        // as a tool, and it is far from the orange the brief rules out.
        p.accent           = oklch(0.70, 0.11, 195);
        p.accentHover      = oklch(0.76, 0.11, 195);
        p.accentSoft       = oklch(0.70, 0.11, 195, 0.18);
        p.accentSoftBorder = oklch(0.70, 0.11, 195, 0.42);

        p.green            = oklch(0.68, 0.11, 152);
        p.greenSoft        = oklch(0.68, 0.11, 152, 0.15);
        p.red              = oklch(0.66, 0.15, 25);
        p.redSoft          = oklch(0.66, 0.15, 25, 0.14);
        p.yellow           = oklch(0.78, 0.12, 95);

        // Syntax follows One Dark: magenta keywords, green strings, warm
        // orange numbers, blue functions, yellow types. Softer and warmer than
        // the previous set, which was pitched at full chroma and made a dense
        // file read as noisy.
        p.synKeyword       = oklch(0.70, 0.14, 315);
        p.synString        = oklch(0.76, 0.13, 140);
        p.synNumber        = oklch(0.77, 0.11, 70);
        p.synFunction      = oklch(0.72, 0.11, 245);
        p.synType          = oklch(0.83, 0.11, 90);
        p.synTag           = oklch(0.67, 0.15, 20);
        p.synComment       = oklch(0.53, 0.015, 264);
        p.synPlain         = oklch(0.90, 0.008, 264);
        p.synPunct         = oklch(0.72, 0.012, 264);
        p.synPreproc       = oklch(0.71, 0.13, 330);
        p.synConstant      = oklch(0.77, 0.10, 70);
        p.synOperator      = oklch(0.74, 0.09, 190);
        p.synAttribute     = oklch(0.80, 0.10, 85);
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

        p.accent           = oklch(0.55, 0.11, 195);
        p.accentHover      = oklch(0.48, 0.11, 195);
        p.accentSoft       = oklch(0.55, 0.11, 195, 0.12);
        p.accentSoftBorder = oklch(0.55, 0.11, 195, 0.34);

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
        p.synPreproc       = oklch(0.5,  0.13, 330);
        p.synConstant      = oklch(0.5,  0.12, 285);
        p.synOperator      = oklch(0.42, 0.05, 220);
        p.synAttribute     = oklch(0.5,  0.1,  90);
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
KEYS_THEME_COLOR(synPreproc)
KEYS_THEME_COLOR(synConstant)
KEYS_THEME_COLOR(synOperator)
KEYS_THEME_COLOR(synAttribute)

#undef KEYS_THEME_COLOR

} // namespace keys::ui
