import QtQuick
import Keys.Ui

/// A button on the welcome screen.
///
/// One primary action per view, in the accent; everything else is a quiet
/// outline. Two buttons competing for the eye leaves the user deciding which
/// one the application wants them to press.
Item {
    id: root

    property string text: ""

    /// The accent fill. Exactly one button on a view should set this.
    property bool primary: false

    signal clicked()

    implicitWidth: label.implicitWidth + Metrics.spacingLarge * 2
    implicitHeight: 32
    width: implicitWidth
    height: implicitHeight

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSmall

        // Pressed goes darker than resting. A button that only lightens on
        // hover gives no feedback at the moment of the click, which is the one
        // moment the user is looking for it.
        color: root.primary
               ? (tap.pressed ? Theme.accentPressed
                  : hover.hovered ? Theme.accentHover : Theme.accent)
               : (hover.hovered ? Theme.bgHover : "transparent")

        border.width: root.primary ? 0 : 1
        border.color: Theme.border

        // Fast enough to feel immediate, slow enough to read as a response.
        // A duration of zero is how the settings express "no animation", so
        // this needs no enabled guard of its own.
        Behavior on color {
            ColorAnimation { duration: App.fastAnimationDuration }
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    Text {
        id: label

        anchors.centerIn: parent
        text: root.text
        color: root.primary ? "#ffffff" : Theme.textSecondary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        font.weight: root.primary ? Font.Medium : Font.Normal
    }

    TapHandler {
        id: tap
        onTapped: root.clicked()
    }
}
