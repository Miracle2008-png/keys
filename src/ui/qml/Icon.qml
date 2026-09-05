import QtQuick
import QtQuick.Shapes

/// Renders an SVG path as a stroked icon that inherits its color from a property.
///
/// Using Shapes rather than image files means one icon serves both themes and
/// every state (hover, active, disabled) without duplicated assets, and stays
/// crisp at any scale factor. Icons are authored on a 24x24 grid and scaled here.
Item {
    id: root

    /// SVG path data, from the Icons singleton.
    property string source: ""

    /// Rendered edge length in logical pixels.
    property int size: 17

    property color color: "white"

    /// Design icons use 1.6-1.8; heavier for chevrons, which are small and would
    /// otherwise disappear.
    property real strokeWidth: 1.6

    /// Some glyphs (the run triangle, status dots) are solid rather than stroked.
    property bool filled: false

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    // The Shape works in the icons' native 24x24 coordinate space and is scaled to
    // the requested size. Authoring and rendering stay decoupled: an icon can be
    // used at any size without editing its path data.
    Shape {
        width: 24
        height: 24

        // Curves in these paths are small; the curve renderer keeps them smooth
        // without the multisampling cost of the geometry renderer.
        preferredRendererType: Shape.CurveRenderer

        transform: Scale {
            xScale: root.size / 24.0
            yScale: root.size / 24.0
        }

        ShapePath {
            strokeColor: root.filled ? "transparent" : root.color
            fillColor: root.filled ? root.color : "transparent"

            // In the 24-unit space, so visual weight stays constant once scaled.
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg { path: root.source }
        }
    }
}
