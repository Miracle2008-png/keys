#include "config/Settings.h"
#include "ui/Metrics.h"
#include "ui/OklchColor.h"
#include "ui/Theme.h"

#include <QSignalSpy>
#include <QTest>

using namespace keys::ui;
using keys::config::Settings;

class ThemeTests : public QObject {
    Q_OBJECT

private:
    /// Colour comparison with a tolerance of one 8-bit step. The conversion is
    /// floating point, so exact equality would be testing the FPU rather than the
    /// implementation.
    static bool near(const QColor& actual, int r, int g, int b, int tolerance = 1)
    {
        return std::abs(actual.red() - r) <= tolerance
            && std::abs(actual.green() - g) <= tolerance
            && std::abs(actual.blue() - b) <= tolerance;
    }

private slots:
    // ---- OKLCH conversion -------------------------------------------------
    // The whole palette is specified in OKLCH, so an error here would tint every
    // surface in the application. These anchor the conversion to known values.

    void convertsPureWhite()
    {
        const QColor white = oklch(1.0, 0.0, 0.0);
        QVERIFY2(near(white, 255, 255, 255), qPrintable(white.name()));
    }

    void convertsPureBlack()
    {
        const QColor black = oklch(0.0, 0.0, 0.0);
        QVERIFY2(near(black, 0, 0, 0), qPrintable(black.name()));
    }

    void convertsMidGreyAchromatically()
    {
        // Zero chroma must produce an exactly neutral colour; any channel drift
        // would tint the entire graphite palette.
        const QColor grey = oklch(0.5, 0.0, 0.0);
        QCOMPARE(grey.red(), grey.green());
        QCOMPARE(grey.green(), grey.blue());
    }

    void convertsKnownSrgbPrimary()
    {
        // sRGB red is OKLCH(0.6280, 0.2577, 29.23) by definition of the transform.
        const QColor red = oklch(0.62796, 0.25768, 29.234);
        QVERIFY2(near(red, 255, 0, 0, 2), qPrintable(red.name()));
    }

    void preservesAlpha()
    {
        const QColor translucent = oklch(0.64, 0.1, 252, 0.16);
        QCOMPARE(translucent.alpha(), 41);  // 0.16 * 255, rounded
    }

    void clampsOutOfGamutValues()
    {
        // Far outside sRGB. Must clamp rather than wrap into a wrong colour.
        const QColor extreme = oklch(0.5, 0.9, 140);
        QVERIFY(extreme.red() >= 0 && extreme.red() <= 255);
        QVERIFY(extreme.green() >= 0 && extreme.green() <= 255);
        QVERIFY(extreme.blue() >= 0 && extreme.blue() <= 255);
    }

    // ---- Theme ------------------------------------------------------------

    void defaultsToDarkMode()
    {
        const Theme theme;
        QCOMPARE(theme.mode(), Theme::Mode::Dark);
    }

    void darkAndLightPalettesDiffer()
    {
        Theme theme;
        const QColor darkChrome = theme.bgChrome();
        const QColor darkText = theme.textPrimary();

        theme.setMode(Theme::Mode::Light);
        QVERIFY(theme.bgChrome() != darkChrome);
        QVERIFY(theme.textPrimary() != darkText);
    }

    void darkThemeIsDarkAndLightThemeIsLight()
    {
        // Guards against the two palettes being transposed, which no individual
        // colour assertion would catch.
        Theme theme;
        theme.setMode(Theme::Mode::Dark);
        QVERIFY(theme.bgChrome().lightness() < 80);
        QVERIFY(theme.textPrimary().lightness() > 180);

        theme.setMode(Theme::Mode::Light);
        QVERIFY(theme.bgChrome().lightness() > 180);
        QVERIFY(theme.textPrimary().lightness() < 80);
    }

    void accentIsBlueInBothModes()
    {
        // The brief forbids an orange accent. Blue means the blue channel leads.
        Theme theme;
        for (const Theme::Mode mode : {Theme::Mode::Dark, Theme::Mode::Light}) {
            theme.setMode(mode);
            const QColor accent = theme.accent();
            QVERIFY2(accent.blue() > accent.red(), qPrintable(accent.name()));
            QVERIFY2(accent.blue() > accent.green(), qPrintable(accent.name()));
        }
    }

    void softAccentIsTranslucent()
    {
        const Theme theme;
        QVERIFY(theme.accentSoft().alpha() < 255);
        QVERIFY(theme.border().alpha() < 255);
    }

    void modeChangeEmitsOnce()
    {
        Theme theme;
        QSignalSpy spy(&theme, &Theme::changed);

        theme.setMode(Theme::Mode::Light);
        QCOMPARE(spy.count(), 1);

        // Setting the mode it already has must stay quiet, or every binding in
        // the interface re-evaluates for nothing.
        theme.setMode(Theme::Mode::Light);
        QCOMPARE(spy.count(), 1);
    }

    void toggleAlternatesModes()
    {
        Theme theme;
        QCOMPARE(theme.mode(), Theme::Mode::Dark);
        theme.toggleMode();
        QCOMPARE(theme.mode(), Theme::Mode::Light);
        theme.toggleMode();
        QCOMPARE(theme.mode(), Theme::Mode::Dark);
    }

    void boundThemeFollowsSettings()
    {
        Settings settings;
        Theme theme;
        theme.bindTo(settings);

        QVERIFY(static_cast<bool>(
            settings.setValue(QStringLiteral("appearance.theme"), QStringLiteral("light"))));
        QCOMPARE(theme.mode(), Theme::Mode::Light);
    }

    void boundThemeWritesBackToSettings()
    {
        // Toggling from the UI must persist, or the choice is lost on restart.
        Settings settings;
        Theme theme;
        theme.bindTo(settings);

        theme.toggleMode();
        QCOMPARE(settings.stringValue(QStringLiteral("appearance.theme")),
                 QStringLiteral("light"));
        QCOMPARE(theme.mode(), Theme::Mode::Light);
    }

    // ---- Metrics ----------------------------------------------------------

    void metricsMatchTheDesign()
    {
        const Metrics metrics;
        QCOMPARE(metrics.topBarHeight, 46);
        QCOMPARE(metrics.statusBarHeight, 24);
        QCOMPARE(metrics.activityRailWidth, 52);
        QCOMPARE(metrics.sidebarDefaultWidth, 268);
        QCOMPARE(metrics.terminalHeaderHeight, 38);
        QCOMPARE(metrics.paletteWidth, 560);
    }

    void metricsHaveNoUnreadablyTinyType()
    {
        // The brief forbids tiny unreadable text. Sizes are stored in points; the
        // design's floor is 11 CSS px for tracked uppercase labels and keycaps,
        // with body text at 12.5. Assert against the design's own numbers by
        // converting back, so this test fails if the scale is ever quietly shrunk.
        const Metrics metrics;
        const auto toCssPx = [](qreal points) { return points / Metrics::kPxToPt; };

        QVERIFY(toCssPx(metrics.fontSizeLabel) >= 10.5);
        QVERIFY(toCssPx(metrics.fontSizeBody) >= 12.0);
        QVERIFY(metrics.fontSizeLabel < metrics.fontSizeBody);
        QVERIFY(metrics.fontSizeBody < metrics.fontSizeTitle);
    }

    void sidebarBoundsAreSane()
    {
        const Metrics metrics;
        QVERIFY(metrics.sidebarMinWidth < metrics.sidebarDefaultWidth);
        QVERIFY(metrics.sidebarDefaultWidth < metrics.sidebarMaxWidth);
    }
};

QTEST_MAIN(ThemeTests)
#include "ThemeTests.moc"
