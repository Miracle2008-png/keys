#pragma once

#include "config/Settings.h"

#include <QColor>
#include <QObject>
#include <QQmlEngine>

namespace keys::ui {

/// The design's color tokens, exposed to QML as a singleton.
///
/// Every color in Keys comes from here. QML never writes a literal color, so
/// switching themes is a property change that propagates through bindings rather
/// than a reload, and the two themes cannot drift apart.
///
/// The design specifies colors in OKLCH. Qt has no OKLCH type, so the values are
/// converted once at construction (see OklchColor) and stored as QColor. Keeping
/// the OKLCH source values in the code means the palette can be compared against
/// the design document directly rather than through hex values nobody can verify.
class Theme : public QObject {
    Q_OBJECT
    // NAMED_ELEMENT with a create() factory, and deliberately not
    // default-constructible: a QML_SINGLETON whose type the engine *can*
    // construct will construct its own instance rather than calling create(),
    // and QML then binds to a second Theme that main() never bound to settings.
    // The symptom is a theme that toggles from the rail (which mutates QML's
    // copy) but never persists and never responds to the settings page.
    QML_NAMED_ELEMENT(Theme)
    QML_SINGLETON

    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY changed)

    // Surfaces, back to front.
    Q_PROPERTY(QColor bgChrome READ bgChrome NOTIFY changed)
    Q_PROPERTY(QColor bgSurface READ bgSurface NOTIFY changed)
    Q_PROPERTY(QColor bgEditor READ bgEditor NOTIFY changed)
    Q_PROPERTY(QColor bgElevated READ bgElevated NOTIFY changed)
    Q_PROPERTY(QColor bgHover READ bgHover NOTIFY changed)

    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor borderStrong READ borderStrong NOTIFY changed)

    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY changed)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY changed)
    Q_PROPERTY(QColor textTertiary READ textTertiary NOTIFY changed)

    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor accentHover READ accentHover NOTIFY changed)
    Q_PROPERTY(QColor accentSoft READ accentSoft NOTIFY changed)
    Q_PROPERTY(QColor accentSoftBorder READ accentSoftBorder NOTIFY changed)

    Q_PROPERTY(QColor green READ green NOTIFY changed)
    Q_PROPERTY(QColor greenSoft READ greenSoft NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)
    Q_PROPERTY(QColor redSoft READ redSoft NOTIFY changed)
    Q_PROPERTY(QColor yellow READ yellow NOTIFY changed)

    // Syntax colors live in the theme so a color scheme is one coherent object
    // rather than a UI palette plus a separate editor palette that can disagree.
    Q_PROPERTY(QColor synKeyword READ synKeyword NOTIFY changed)
    Q_PROPERTY(QColor synString READ synString NOTIFY changed)
    Q_PROPERTY(QColor synNumber READ synNumber NOTIFY changed)
    Q_PROPERTY(QColor synFunction READ synFunction NOTIFY changed)
    Q_PROPERTY(QColor synType READ synType NOTIFY changed)
    Q_PROPERTY(QColor synTag READ synTag NOTIFY changed)
    Q_PROPERTY(QColor synComment READ synComment NOTIFY changed)
    Q_PROPERTY(QColor synPlain READ synPlain NOTIFY changed)
    Q_PROPERTY(QColor synPunct READ synPunct NOTIFY changed)

public:
    enum class Mode { Dark, Light };
    Q_ENUM(Mode)

    /// Takes the settings it is bound to. Required rather than defaulted so the
    /// QML engine cannot construct one: see the note on QML_SINGLETON above.
    explicit Theme(config::Settings& settings, QObject* parent = nullptr);

    /// Publishes the application's Theme to QML. Called from main() before the
    /// engine loads, so QML resolves the singleton to the instance the
    /// application owns rather than constructing a second, unbound one.
    static void setInstance(Theme* instance);

    /// QML singleton factory. Returns the instance published by setInstance and
    /// leaves ownership in C++, so the engine cannot delete an object main() owns.
    static Theme* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] Mode mode() const { return m_mode; }
    void setMode(Mode mode);

    Q_INVOKABLE void toggleMode();

    [[nodiscard]] QColor bgChrome() const;
    [[nodiscard]] QColor bgSurface() const;
    [[nodiscard]] QColor bgEditor() const;
    [[nodiscard]] QColor bgElevated() const;
    [[nodiscard]] QColor bgHover() const;

    [[nodiscard]] QColor border() const;
    [[nodiscard]] QColor borderStrong() const;

    [[nodiscard]] QColor textPrimary() const;
    [[nodiscard]] QColor textSecondary() const;
    [[nodiscard]] QColor textTertiary() const;

    [[nodiscard]] QColor accent() const;
    [[nodiscard]] QColor accentHover() const;
    [[nodiscard]] QColor accentSoft() const;
    [[nodiscard]] QColor accentSoftBorder() const;

    [[nodiscard]] QColor green() const;
    [[nodiscard]] QColor greenSoft() const;
    [[nodiscard]] QColor red() const;
    [[nodiscard]] QColor redSoft() const;
    [[nodiscard]] QColor yellow() const;

    [[nodiscard]] QColor synKeyword() const;
    [[nodiscard]] QColor synString() const;
    [[nodiscard]] QColor synNumber() const;
    [[nodiscard]] QColor synFunction() const;
    [[nodiscard]] QColor synType() const;
    [[nodiscard]] QColor synTag() const;
    [[nodiscard]] QColor synComment() const;
    [[nodiscard]] QColor synPlain() const;
    [[nodiscard]] QColor synPunct() const;

signals:
    void changed();

private:
    void applyFromSettings(const QVariant& value);

    config::Settings& m_settings;
    Mode m_mode = Mode::Dark;
};

} // namespace keys::ui
