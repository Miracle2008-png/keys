import QtQuick
import QtQuick.Controls
import Keys.Ui

/// One readout in the status bar.
///
/// Dim by default and brighter on hover, so the bar reads as a row of things
/// you *can* look at rather than a row of things demanding to be read. The
/// tooltip says what a terse value means: "3:17" is obvious once you know and
/// opaque until then.
Text {
    id: root

    /// What the value is, shown on hover.
    property string tip: ""

    color: hover.hovered ? Theme.textSecondary : Theme.textTertiary
    font.family: Fonts.ui
    font.pointSize: Metrics.fontSizeLabel
    anchors.verticalCenter: parent ? parent.verticalCenter : undefined

    Behavior on color {
        ColorAnimation { duration: App.fastAnimationDuration }
    }

    HoverHandler {
        id: hover
    }

    ToolTip.visible: hover.hovered && root.tip.length > 0
    ToolTip.text: root.tip
    ToolTip.delay: 500
}
