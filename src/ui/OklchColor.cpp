#include "ui/OklchColor.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace keys::ui {
namespace {

/// The sRGB electro-optical transfer function, encoding a linear component.
double linearToSrgb(double x)
{
    if (x <= 0.0031308) {
        return 12.92 * x;
    }
    return 1.055 * std::pow(x, 1.0 / 2.4) - 0.055;
}

double clamp01(double x)
{
    return std::clamp(x, 0.0, 1.0);
}

} // namespace

QColor oklch(double l, double c, double h, double alpha)
{
    // OKLCH is the polar form of OKLab: chroma and hue become the a/b axes.
    const double hRadians = h * std::numbers::pi_v<double> / 180.0;
    const double a = c * std::cos(hRadians);
    const double b = c * std::sin(hRadians);

    // OKLab -> LMS (cone response), via the inverse of Oklab's second matrix.
    const double lRoot = l + 0.3963377774 * a + 0.2158037573 * b;
    const double mRoot = l - 0.1055613458 * a - 0.0638541728 * b;
    const double sRoot = l - 0.0894841775 * a - 1.2914855480 * b;

    // Oklab operates on the cube roots of the cone responses; undo that.
    const double lCone = lRoot * lRoot * lRoot;
    const double mCone = mRoot * mRoot * mRoot;
    const double sCone = sRoot * sRoot * sRoot;

    // LMS -> linear sRGB.
    const double rLinear =
        +4.0767416621 * lCone - 3.3077115913 * mCone + 0.2309699292 * sCone;
    const double gLinear =
        -1.2684380046 * lCone + 2.6097574011 * mCone - 0.3413193965 * sCone;
    const double bLinear =
        -0.0041960863 * lCone - 0.7034186147 * mCone + 1.7076147010 * sCone;

    // Clamping after encoding rather than before keeps in-gamut colors exact;
    // the design's palette never relies on this path.
    QColor result;
    result.setRgbF(static_cast<float>(clamp01(linearToSrgb(rLinear))),
                   static_cast<float>(clamp01(linearToSrgb(gLinear))),
                   static_cast<float>(clamp01(linearToSrgb(bLinear))),
                   static_cast<float>(clamp01(alpha)));
    return result;
}

} // namespace keys::ui
