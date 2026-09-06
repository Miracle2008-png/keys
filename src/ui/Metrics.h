#pragma once

#include <QObject>
#include <QQmlEngine>

namespace keys::ui {

/// Layout constants taken from the design, exposed to QML as a singleton.
///
/// These are dimensions the design fixes rather than derives — rail width, bar
/// heights, corner radii, type sizes. Keeping them here means a QML file never
/// hard-codes 52 or 268, and a design revision is one edit rather than a search
/// across every view.
///
/// Sizes are in logical pixels; Qt applies device pixel ratio on top.
class Metrics : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // ---- Chrome ----------------------------------------------------------
    Q_PROPERTY(int topBarHeight MEMBER topBarHeight CONSTANT)
    Q_PROPERTY(int statusBarHeight MEMBER statusBarHeight CONSTANT)
    Q_PROPERTY(int activityRailWidth MEMBER activityRailWidth CONSTANT)
    Q_PROPERTY(int sidebarDefaultWidth MEMBER sidebarDefaultWidth CONSTANT)
    Q_PROPERTY(int sidebarMinWidth MEMBER sidebarMinWidth CONSTANT)
    Q_PROPERTY(int sidebarMaxWidth MEMBER sidebarMaxWidth CONSTANT)
    Q_PROPERTY(int terminalHeaderHeight MEMBER terminalHeaderHeight CONSTANT)
    Q_PROPERTY(int breadcrumbHeight MEMBER breadcrumbHeight CONSTANT)

    // ---- Controls --------------------------------------------------------
    Q_PROPERTY(int railButtonSize MEMBER railButtonSize CONSTANT)
    Q_PROPERTY(int iconButtonSize MEMBER iconButtonSize CONSTANT)
    Q_PROPERTY(int searchFieldWidth MEMBER searchFieldWidth CONSTANT)
    Q_PROPERTY(int paletteWidth MEMBER paletteWidth CONSTANT)
    Q_PROPERTY(int paletteMaxHeight MEMBER paletteMaxHeight CONSTANT)

    // ---- Radii -----------------------------------------------------------
    // The brief asks for moderate rounding; these are the design's values.
    Q_PROPERTY(int radiusSmall MEMBER radiusSmall CONSTANT)
    Q_PROPERTY(int radiusMedium MEMBER radiusMedium CONSTANT)
    Q_PROPERTY(int radiusLarge MEMBER radiusLarge CONSTANT)

    // ---- Spacing ---------------------------------------------------------
    Q_PROPERTY(int rowHeight MEMBER rowHeight CONSTANT)
    Q_PROPERTY(int spacingTight MEMBER spacingTight CONSTANT)
    Q_PROPERTY(int spacingSmall MEMBER spacingSmall CONSTANT)
    Q_PROPERTY(int spacingMedium MEMBER spacingMedium CONSTANT)
    Q_PROPERTY(int spacingLarge MEMBER spacingLarge CONSTANT)

    // ---- Type ------------------------------------------------------------
    // The design specifies fractional sizes (10.5, 12.5, 13.5). Qt's
    // font.pixelSize is an integer, so these are exposed as reals and assigned
    // to font.pointSize, which does accept fractions. Points are converted
    // against the screen's logical DPI, so text also scales correctly on
    // high-DPI displays instead of being pinned to device pixels.
    //
    // The design's smallest size is 10.5, used only for keycaps and tracked
    // uppercase labels. Body text is 12.5-13.5.
    Q_PROPERTY(qreal fontSizeLabel MEMBER fontSizeLabel CONSTANT)
    Q_PROPERTY(qreal fontSizeSmall MEMBER fontSizeSmall CONSTANT)
    Q_PROPERTY(qreal fontSizeBody MEMBER fontSizeBody CONSTANT)
    Q_PROPERTY(qreal fontSizeMedium MEMBER fontSizeMedium CONSTANT)
    Q_PROPERTY(qreal fontSizeLarge MEMBER fontSizeLarge CONSTANT)
    Q_PROPERTY(qreal fontSizeTitle MEMBER fontSizeTitle CONSTANT)

public:
    /// The design's sizes are CSS pixels. A CSS pixel is 1/96 inch and a point is
    /// 1/72, so a size in points is the design's value times 72/96. Converting
    /// here keeps the design's own numbers legible in the declarations below
    /// while rendering at the intended visual size.
    static constexpr qreal kPxToPt = 0.75;

    explicit Metrics(QObject* parent = nullptr) : QObject(parent) {}

    // Denser than before, against CLion's own proportions. A 46px top bar and
    // a 52px rail spent a strip of every screen on chrome; the point of an IDE
    // window is how much of the project it can show at once, and every row
    // saved here is a line of code gained.
    int topBarHeight = 36;
    int statusBarHeight = 24;
    int activityRailWidth = 44;
    int sidebarDefaultWidth = 248;
    int sidebarMinWidth = 180;
    int sidebarMaxWidth = 600;
    int terminalHeaderHeight = 32;
    int breadcrumbHeight = 26;

    int railButtonSize = 30;
    int iconButtonSize = 26;
    int searchFieldWidth = 340;
    int paletteWidth = 560;
    int paletteMaxHeight = 420;

    // Tight. CLion, RustRover and Atom all sit at 2-4px; 6-12 is web-app
    // rounding, and it is most of why Keys read as a site in a window rather
    // than a tool. A menu row at radius 6 looks like a pill; at 3 it looks
    // like a row that happens to be highlighted, which is what it is.
    int radiusSmall = 3;
    int radiusMedium = 4;
    int radiusLarge = 6;

    /// The height of a row in a tree, a list or a menu. One number, so the
    /// explorer, the menus and the palette share a rhythm rather than each
    /// choosing its own.
    int rowHeight = 24;

    int spacingTight = 4;
    int spacingSmall = 8;
    int spacingMedium = 14;
    int spacingLarge = 20;

    qreal fontSizeLabel = 11.0 * kPxToPt;
    qreal fontSizeSmall = 11.5 * kPxToPt;
    qreal fontSizeBody = 12.5 * kPxToPt;
    qreal fontSizeMedium = 13.0 * kPxToPt;
    qreal fontSizeLarge = 13.5 * kPxToPt;
    qreal fontSizeTitle = 17.0 * kPxToPt;
};

} // namespace keys::ui
