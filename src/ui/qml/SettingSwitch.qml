import QtQuick
import Keys.Ui

/// A boolean toggle.
///
/// Hand-drawn rather than styling QtQuick.Controls' Switch: the Basic style's
/// indicator carries its own metrics and colours, and overriding them costs more
/// than the forty lines here - which read as the design describes them.
Item {
    id: root

    required property bool checked

    signal toggled(bool value)

    implicitWidth: 36
    implicitHeight: 20

    Rectangle {
        id: track

        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.accent : Theme.bgHover
        border.width: root.checked ? 0 : 1
        border.color: Theme.border

        Behavior on color {
            ColorAnimation { duration: App.fastAnimationDuration }
        }
    }

    Rectangle {
        id: knob

        width: 14
        height: 14
        radius: height / 2
        anchors.verticalCenter: parent.verticalCenter
        x: root.checked ? parent.width - width - 3 : 3
        color: root.checked ? "#ffffff" : Theme.textTertiary

        Behavior on x {
            NumberAnimation {
                duration: App.fastAnimationDuration
                easing.type: Easing.OutCubic
            }
        }

        Behavior on color {
            ColorAnimation { duration: App.fastAnimationDuration }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
