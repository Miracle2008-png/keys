#include "config/AnimationPolicy.h"

namespace keys::config {
namespace {

constexpr auto kAnimationLevelKey = "animation.level";

// The design's own timings: .15s full, .06s reduced, 0s off.
constexpr int kFullDurationMs = 150;
constexpr int kReducedDurationMs = 60;

// Hover and focus feedback needs to land well inside a frame of the user's
// attention; the standard duration reads as lag on these.
constexpr int kFullFastDurationMs = 90;
constexpr int kReducedFastDurationMs = 40;

} // namespace

AnimationPolicy::AnimationPolicy(Settings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
    applyFromSettings();

    connect(&m_settings, &Settings::changed, this,
            [this](const QString& key, const QVariant&) {
                if (key == QLatin1String(kAnimationLevelKey)) {
                    applyFromSettings();
                }
            });
}

int AnimationPolicy::duration() const
{
    switch (m_level) {
    case Level::Full:
        return kFullDurationMs;
    case Level::Reduced:
        return kReducedDurationMs;
    case Level::Off:
        return 0;
    }
    return kFullDurationMs;
}

int AnimationPolicy::fastDuration() const
{
    switch (m_level) {
    case Level::Full:
        return kFullFastDurationMs;
    case Level::Reduced:
        return kReducedFastDurationMs;
    case Level::Off:
        return 0;
    }
    return kFullFastDurationMs;
}

AnimationPolicy::Level AnimationPolicy::levelFromString(const QString& text)
{
    const QString normalized = text.toLower();
    if (normalized == QLatin1String("reduced")) {
        return Level::Reduced;
    }
    if (normalized == QLatin1String("off")) {
        return Level::Off;
    }
    return Level::Full;
}

QString AnimationPolicy::levelToString(Level level)
{
    switch (level) {
    case Level::Reduced:
        return QStringLiteral("reduced");
    case Level::Off:
        return QStringLiteral("off");
    case Level::Full:
        break;
    }
    return QStringLiteral("full");
}

bool AnimationPolicy::systemPrefersReducedMotion()
{
    // Qt exposes this through the platform theme, but only in the GUI layer, and
    // config must not link QtGui (docs/ARCHITECTURE.md §2). The UI layer reads the
    // platform preference and writes the setting; this hook exists so the policy
    // has one documented home. Until the UI supplies it, assume no preference.
    return false;
}

void AnimationPolicy::applyFromSettings()
{
    const Level resolved =
        levelFromString(m_settings.stringValue(QLatin1String(kAnimationLevelKey)));
    if (resolved != m_level) {
        m_level = resolved;
        emit changed();
    }
}

} // namespace keys::config
