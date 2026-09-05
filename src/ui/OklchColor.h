#pragma once

#include <QColor>

namespace keys::ui {

/// Converts an OKLCH color to sRGB.
///
/// The design specifies its entire palette in OKLCH, which Qt has no type for.
/// Rather than paste in hex values that nobody can check against the design, Keys
/// keeps the OKLCH triples in the source and converts them here — so the palette
/// in Theme.cpp can be diffed against the design document line by line.
///
/// The pipeline is the standard one: OKLCH → OKLab → linear sRGB → gamma-encoded
/// sRGB, per Björn Ottosson's definition of Oklab and the sRGB transfer function.
///
/// \param l  Lightness, 0..1
/// \param c  Chroma, typically 0..0.4
/// \param h  Hue in degrees, 0..360
/// \param alpha Opacity, 0..1
///
/// Out-of-gamut results are clamped per channel. Every color in the design's
/// palette is well inside sRGB, so clamping is a guard, not a routine operation.
[[nodiscard]] QColor oklch(double l, double c, double h, double alpha = 1.0);

} // namespace keys::ui
