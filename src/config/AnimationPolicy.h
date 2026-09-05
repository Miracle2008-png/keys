#pragma once

#include "config/Settings.h"

#include <QObject>

namespace keys::config {

/// The single source of truth for animation timing across the application.
///
/// The design expresses motion through one token (--anim-t). Keys mirrors that:
/// every animation reads its duration from here rather than hard-coding one, so
/// the Full/Reduced/Off preference genuinely governs the whole interface instead
/// of the handful of places someone remembered to wire up.
///
/// "Off" disables animation rather than shortening it — a 1 ms animation still
/// schedules work and still lands mid-frame, which is exactly what a user who
/// turned motion off is asking not to have.
class AnimationPolicy : public QObject {
    Q_OBJECT

    Q_PROPERTY(int duration READ duration NOTIFY changed)
    Q_PROPERTY(int fastDuration READ fastDuration NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)

public:
    enum class Level { Full, Reduced, Off };
    Q_ENUM(Level)

    explicit AnimationPolicy(Settings& settings, QObject* parent = nullptr);

    [[nodiscard]] Level level() const { return m_level; }

    /// Standard transition length in milliseconds: 150 / 60 / 0.
    /// These are the design's own values for full and reduced motion.
    [[nodiscard]] int duration() const;

    /// For small state changes - hover, focus - where the standard duration reads
    /// as sluggish. Always shorter than duration(), and still zero when off.
    [[nodiscard]] int fastDuration() const;

    [[nodiscard]] bool enabled() const { return m_level != Level::Off; }

    /// Parses "full" / "reduced" / "off". Unrecognised input yields Full, matching
    /// the schema default.
    [[nodiscard]] static Level levelFromString(const QString& text);
    [[nodiscard]] static QString levelToString(Level level);

    /// True when the operating system asks applications to reduce motion. Used to
    /// pick the initial default so Keys honours the system preference on first run
    /// without overriding an explicit choice later.
    [[nodiscard]] static bool systemPrefersReducedMotion();

signals:
    void changed();

private:
    void applyFromSettings();

    Settings& m_settings;
    Level m_level = Level::Full;
};

} // namespace keys::config
